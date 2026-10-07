# Sobel Edge Detection on Zynq FPGA using AXI DMA & AXI4-Stream

Zybo Z7-10의 **Zynq PS + AXI DMA + AXI4-Stream + custom Sobel RTL IP**를 이용해 구현한 FPGA 이미지 처리 프로젝트입니다.

640×480 8-bit grayscale 이미지를 DDR에서 AXI DMA를 통해 PL로 전달하고, custom Sobel IP에서 **2-line buffer 기반 3×3 sliding window**와 Sobel convolution을 수행한 뒤 결과를 다시 DDR로 반환합니다. FPGA 출력은 동일한 연산과 border behavior를 구현한 Python reference model과 pixel-by-pixel로 비교했습니다.

> **Verified build:** 현재 제공된 `design_1_wrapper.xsa`에서 `FCLK_CLK0`, AXI DMA, Sobel IP clock은 **50 MHz**입니다. 따라서 throughput 계산은 이 50 MHz build를 기준으로 합니다.

## Project Highlights

- Zynq PS ↔ DDR ↔ AXI DMA ↔ PL 데이터 경로 구성
- 32-bit AXI4-Stream 기반 custom Sobel RTL IP
- 8-bit grayscale pixel을 **32-bit AXI word의 LSB에 1 pixel/word로 매핑**
- 2-line buffer + horizontal shift registers 기반 3×3 sliding window
- `|Gx| + |Gy|` 기반 gradient magnitude approximation
- AXI4-Stream backpressure를 고려한 `TVALID/TREADY` handshake
- 첫 2행/2열에서 zero output을 생성해 input/output beat 수 유지
- stall이 없는 steady state에서 **1 accepted pixel / cycle**
- Python reference와 FPGA output의 pixel-by-pixel comparison
- Vivado/Vitis 기반 Zynq hardware/software integration

---

## 1. System Architecture

![Zybo Z7-10 Sobel system](https://velog.velcdn.com/images/vom/post/0d167f41-6610-4259-8648-cb02060fd64a/image.jpg)

```text
8-bit grayscale image
        ↓
Python -> image_data.h
        ↓
Zynq PS
        ↓  1 pixel -> 32-bit word (0x000000PP)
DDR TX buffer
        ↓ AXI DMA MM2S
32-bit AXI4-Stream
        ↓
Sobel RTL IP
        ↓
32-bit AXI4-Stream
        ↓ AXI DMA S2MM
DDR RX buffer
        ↓  low byte extraction
Zynq PS
        ↓ UART (1 byte/pixel)
PC -> output PNG
```

![System architecture](https://velog.velcdn.com/images/vom/post/eec8feff-a40b-42e8-8d8e-6940deb953b9/image.png)

### Verified hardware configuration

| Item | Configuration |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| Vivado | 2025.2 |
| Sobel / DMA clock | 50 MHz |
| AXI DMA mode | Simple DMA, SG disabled |
| MM2S stream width | 32 bit |
| S2MM stream width | 32 bit |
| DMA length width | 23 bit |
| Sobel `DATA_WIDTH` | 32 bit |
| Sobel `IMAGE_WIDTH` | 640 |

---

## 2. PS-side Pixel Packing and DMA Transfer

The source image contains 307,200 8-bit pixels:

```text
640 × 480 = 307,200 pixels
```

Because the DMA stream width is 32 bits, the Vitis application expands each grayscale pixel to one 32-bit word:

```c
TxBufferPtr[i] = (u32)image_data[i];
```

Memory/stream representation:

```text
0x000000PP
```

Therefore:

```text
307,200 pixels
× 4 bytes / AXI word
= 1,228,800 bytes per DMA direction
```

The S2MM channel is armed before MM2S so that the receive path is ready before the Sobel core produces output. The software flushes the DMA buffers before transfer and invalidates the RX cache after completion.

After DMA completion, only the low byte of each 32-bit result word is sent over UART:

```c
outbyte((char)(RxBufferPtr[i] & 0xFFU));
```

The PC-side capture script receives exactly `width × height` bytes and reconstructs an 8-bit grayscale PNG.

---

## 3. Sobel RTL Datapath

```text
AXI4-Stream Input
        ↓
2-Line Buffer
        ↓
3×3 Sliding Window
        ↓
Gx / Gy Add-Shift-Subtract Datapath
        ↓
|Gx| + |Gy|
        ↓
8-bit Saturation
        ↓
AXI4-Stream Output
```

The core does not buffer the entire frame. Two previous rows are retained in line buffers and combined with horizontal shift registers to generate a 3×3 neighborhood for each accepted input pixel.

### 3.1 AXI4-Stream flow control

```text
s_axis_tready = !m_axis_tvalid_reg || m_axis_tready
```

When downstream logic deasserts `m_axis_tready`, the core stops accepting new beats and holds the line-buffer/window/counter state. `TLAST` is propagated with the corresponding accepted beat.

### 3.2 Line Buffer & Sliding Window

```text
row y-2 ── line buffer ──┐
row y-1 ── line buffer ──┼── 3×3 window
row y   ── current pixel ┘
```

The current pixel is the bottom-right point of the effective 3×3 window. For `x < 2` or `y < 2`, a complete window is not yet available, so the core outputs zero while preserving the total stream length.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/02839836-1281-4437-99a9-ee11128f04ac/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/040a22d3-8f28-4e1c-98d3-8716f31806d9/image.jpg" width="49%">
</p>

### 3.3 Sobel Convolution

```text
Gx = [ -1   0   1 ]      Gy = [ -1  -2  -1 ]
     [ -2   0   2 ]           [  0   0   0 ]
     [ -1   0   1 ]           [  1   2   1 ]
```

The RTL uses the hardware-friendly approximation:

```text
magnitude = |Gx| + |Gy|
```

and saturates values above 255 to `8'hFF`. The coefficients are implemented with add/subtract and shifts rather than general-purpose multipliers.

---

## 4. Throughput

The datapath can accept one pixel beat per cycle when `TVALID/TREADY` remain continuously asserted.

For the verified 50 MHz build:

```text
307,200 beats / 50,000,000 cycles/s
= 6.144 ms
```

| Metric | Value |
|---|---:|
| Resolution | 640 × 480 |
| Pixels per frame | 307,200 |
| Verified core clock | 50 MHz |
| Steady-state throughput | 1 accepted pixel / cycle |
| Ideal core stream time | 6.144 ms / frame |
| DMA bytes / direction | 1,228,800 bytes |

> `6.144 ms` is the ideal Sobel-core streaming time only. It excludes DMA setup, DDR access variation, PS software overhead, stalls, and UART transfer time.

---

## 5. Functional Verification

The Python flow performs:

```text
input PNG
  ↓ grayscale conversion
  ↓ resize to 640×480
  ↓ C header generation
FPGA processing
  ↓ UART capture
FPGA result PNG
  ↓
Python Sobel reference
  ↓
pixel-by-pixel difference check
```

The Python reference reproduces the RTL window alignment, `|Gx| + |Gy|`, saturation, and first-two-row/column zero behavior.

| Python Reference | FPGA Output |
|---|---|
| ![Python reference](https://velog.velcdn.com/images/vom/post/ddf5f546-1725-4d1b-a493-322bdaab7879/image.png) | ![FPGA output](https://velog.velcdn.com/images/vom/post/f9004dee-1a3b-47db-af3e-9ab7005b6525/image.png) |

![Difference check](https://velog.velcdn.com/images/vom/post/d22e4336-0e3f-42b3-bd4d-486c2630d65b/image.png)

Reported test result:

```text
max error      : 0
mismatch count : 0
mismatch ratio : 0%
```

---

## 6. Implementation Results

Recorded Vivado implementation values:

| Item | Reported Result |
|---|---:|
| WNS | 1.702 ns |
| TNS | 0 ns |
| LUT | 6,659 / 17,600 (37.84%) |
| FF | 13,990 / 35,200 (39.74%) |
| BRAM | 2 / 60 (3.33%) |
| On-Chip Power | 1.535 W |

![Vivado implementation result](https://velog.velcdn.com/images/vom/post/15c43b43-1508-4b1a-b43f-9690c00294e2/image.png)

The currently provided XSA is a 50 MHz build. The original timing/utilization report should be committed before attributing the reported WNS to a different target frequency.

---

## 7. Current Design Scope / Limitation

The current RTL is intended for a **single frame transaction after reset**. `TLAST` is propagated to the DMA, but the internal frame state (`y_cnt` and line-buffer history) is not reinitialized on frame completion.

Therefore continuous multi-frame operation without reset would require explicit frame-boundary state handling. This is a clear extension point for a future revision.

Another optimization point is the line-buffer implementation. The current RTL resets every line-buffer element explicitly; a BRAM-oriented implementation could be redesigned to improve memory inference and reduce register/LUT pressure.

---

## 8. Repository Structure

```text
Sobel-Edge-Detection-RTL/
├── README.md
├── rtl/
│   └── sobel_axis_core.v
├── software/
│   └── main.c
├── python/
│   ├── png_to_header.py
│   ├── imgtest.py
│   └── capture_uart_image.py
├── constraints/
├── docs/
└── assets/
```

Generated Vivado cache/run directories and generated `image_data.h` are intentionally excluded from the portfolio source tree.

---

## 9. Portfolio Focus

```text
Image algorithm
    ↓
Streaming hardware datapath
    ↓
Line-buffer based data reuse
    ↓
AXI4-Stream flow control
    ↓
32-bit DMA / 8-bit pixel interface mapping
    ↓
Zynq PS-PL integration
    ↓
Python reference verification
    ↓
Implementation / timing / resource analysis
```

This project demonstrates **algorithm-to-RTL conversion and streaming FPGA datapath design**, including the software/hardware boundary required to move real image data through a Zynq system.
