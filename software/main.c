#include <stdio.h>
#include "xaxidma.h"
#include "xaxidma_hw.h"
#include "xgpio.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xil_cache.h"
#include "xil_types.h"
#include "image_data.h"

#define MEM_BASE_ADDR 0x01000000U

#define TX_BUFFER_BASE (MEM_BASE_ADDR + 0x00100000U)
#define RX_BUFFER_BASE (MEM_BASE_ADDR + 0x00300000U)

#define WIDTH  IMG_WIDTH
#define HEIGHT IMG_HEIGHT
#define LENGTH (WIDTH * HEIGHT)

#define TX_BYTES (LENGTH * 4)
#define RX_BYTES (LENGTH * 4)

/*
debug_sel meaning
0 : mag
1 : bot_new
2 : mid_new
3 : top_new
4 : w11
5 : w12
6 : abs_gx
7 : abs_gy
8 : constant 0xAA
*/
#define DEBUG_SEL_MODE 0

XAxiDma AxiDma;
XGpio   Gpio;

extern void outbyte(char c);

static int init_gpio(void)
{
    int Status;

    Status = XGpio_Initialize(&Gpio, XPAR_AXI_GPIO_0_BASEADDR);
    if (Status != XST_SUCCESS) {
        printf("GPIO init failed\r\n");
        return XST_FAILURE;
    }

    XGpio_SetDataDirection(&Gpio, 1, 0x0);
    return XST_SUCCESS;
}

static void sobel_set_debug_sel(u32 mode)
{
    XGpio_DiscreteWrite(&Gpio, 1, mode & 0xF);
}

int main(void)
{
    int i;
    int Status;
    int timeout;
    XAxiDma_Config *CfgPtr;

    u32 *TxBufferPtr = (u32 *)TX_BUFFER_BASE;
    u32 *RxBufferPtr = (u32 *)RX_BUFFER_BASE;

    printf("DMA Sobel Test Start\r\n");
    printf("Image size: %d x %d\r\n", WIDTH, HEIGHT);
    printf("Pixel count: %d\r\n", LENGTH);
    printf("TX bytes: %d\r\n", TX_BYTES);
    printf("RX bytes: %d\r\n", RX_BYTES);

    Status = init_gpio();
    if (Status != XST_SUCCESS) {
        return XST_FAILURE;
    }

    sobel_set_debug_sel(DEBUG_SEL_MODE);
    printf("debug_sel = %d\r\n", DEBUG_SEL_MODE);

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

    printf("Expanding input buffer to 32-bit per pixel...\r\n");
    for (i = 0; i < LENGTH; i++) {
        /* little-endian memory view: [pixel][00][00][00] */
        TxBufferPtr[i] = (u32)image_data[i];
        RxBufferPtr[i] = 0U;
    }

    Xil_DCacheFlushRange((UINTPTR)TxBufferPtr, TX_BYTES);
    Xil_DCacheFlushRange((UINTPTR)RxBufferPtr, RX_BYTES);

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

    timeout = 0xffffffff;
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DMA_TO_DEVICE) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        printf("TX DMA timeout\r\n");
        return XST_FAILURE;
    }

    timeout = 0xffffffff;
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        printf("RX DMA timeout\r\n");
        return XST_FAILURE;
    }

    Xil_DCacheInvalidateRange((UINTPTR)RxBufferPtr, RX_BYTES);

    printf("Transfer done\r\n");
    printf("IMG %d %d\r\n", WIDTH, HEIGHT);

    /* LSB 1byte만 UART로 전송 */
    for (i = 0; i < LENGTH; i++) {
        outbyte((char)(RxBufferPtr[i] & 0xFF));
    }

    printf("\r\nDONE\r\n");
    return XST_SUCCESS;
}
