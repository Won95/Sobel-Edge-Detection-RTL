#include <stdio.h>
#include "xaxidma.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xil_cache.h"
#include "xil_types.h"
#include "image_data.h"

#define MEM_BASE_ADDR   0x01000000U
#define TX_BUFFER_BASE  (MEM_BASE_ADDR + 0x00100000U)
#define RX_BUFFER_BASE  (MEM_BASE_ADDR + 0x00300000U)

#define WIDTH            IMG_WIDTH
#define HEIGHT           IMG_HEIGHT
#define PIXEL_COUNT      (WIDTH * HEIGHT)
#define DMA_WORD_BYTES   4U
#define TX_BYTES         (PIXEL_COUNT * DMA_WORD_BYTES)
#define RX_BYTES         (PIXEL_COUNT * DMA_WORD_BYTES)

XAxiDma AxiDma;

extern void outbyte(char c);

int main(void)
{
    int i;
    int Status;
    u32 timeout;
    XAxiDma_Config *CfgPtr;

    u32 *TxBufferPtr = (u32 *)TX_BUFFER_BASE;
    u32 *RxBufferPtr = (u32 *)RX_BUFFER_BASE;

    printf("DMA Sobel Test Start\r\n");
    printf("Image size: %d x %d\r\n", WIDTH, HEIGHT);
    printf("Pixel count: %d\r\n", PIXEL_COUNT);
    printf("TX bytes: %d\r\n", TX_BYTES);
    printf("RX bytes: %d\r\n", RX_BYTES);

    CfgPtr = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
    if (CfgPtr == NULL) {
        printf("No DMA config found\r\n");
        return XST_FAILURE;
    }

    Status = XAxiDma_CfgInitialize(&AxiDma, CfgPtr);
    if (Status != XST_SUCCESS) {
        printf("DMA init failed\r\n");
        return XST_FAILURE;
    }

    if (XAxiDma_HasSg(&AxiDma)) {
        printf("DMA is in SG mode, expected simple mode\r\n");
        return XST_FAILURE;
    }

    /*
     * AXI DMA stream width is 32 bits, while one grayscale pixel is 8 bits.
     * Store one pixel in the LSB of each 32-bit word:
     *
     *   memory/stream word = 0x000000PP
     *
     * Therefore 640x480 pixels correspond to 307,200 AXI stream beats and
     * 307,200 * 4 bytes of DMA memory traffic in each direction.
     */
    for (i = 0; i < PIXEL_COUNT; i++) {
        TxBufferPtr[i] = (u32)image_data[i];
        RxBufferPtr[i] = 0U;
    }

    Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, TX_BYTES);
    Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, RX_BYTES);

    /* Arm S2MM before starting MM2S so the output path is ready first. */
    Status = XAxiDma_SimpleTransfer(&AxiDma,
                                    (UINTPTR)RxBufferPtr,
                                    RX_BYTES,
                                    XAXIDMA_DEVICE_TO_DMA);
    if (Status != XST_SUCCESS) {
        printf("RX transfer setup failed\r\n");
        return XST_FAILURE;
    }

    Status = XAxiDma_SimpleTransfer(&AxiDma,
                                    (UINTPTR)TxBufferPtr,
                                    TX_BYTES,
                                    XAXIDMA_DMA_TO_DEVICE);
    if (Status != XST_SUCCESS) {
        printf("TX transfer setup failed\r\n");
        return XST_FAILURE;
    }

    timeout = 0xFFFFFFFFU;
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE) && timeout > 0U) {
        timeout--;
    }
    if (timeout == 0U) {
        printf("TX DMA timeout\r\n");
        return XST_FAILURE;
    }

    timeout = 0xFFFFFFFFU;
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA) && timeout > 0U) {
        timeout--;
    }
    if (timeout == 0U) {
        printf("RX DMA timeout\r\n");
        return XST_FAILURE;
    }

    Xil_DCacheInvalidateRange((UINTPTR)RxBufferPtr, RX_BYTES);

    printf("Transfer done\r\n");
    printf("IMG %d %d\r\n", WIDTH, HEIGHT);

    /* Convert each 32-bit result word back to one grayscale byte for UART. */
    for (i = 0; i < PIXEL_COUNT; i++) {
        outbyte((char)(RxBufferPtr[i] & 0xFFU));
    }

    printf("\r\nDONE\r\n");
    return XST_SUCCESS;
}
