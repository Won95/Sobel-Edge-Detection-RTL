# Sobel Edge Detection on Zynq FPGA using AXI DMA & AXI4-Stream

Zybo Z7-10의 **Zynq PS + AXI DMA + AXI4-Stream + custom Sobel RTL IP**를 이용해 구현한 FPGA 이미지 처리 프로젝트입니다.

640×480 8-bit grayscale 이미지를 DDR에서 AXI DMA를 통해 PL로 전달하고, custom Sobel IP에서 **2-line buffer 기반 3×3 sliding window**와 Sobel convolution을 수행한 뒤 결과를 다시 DDR로 반환합니다. FPGA 출력은 동일한 연산과 border behavior를 구현한 Python reference model과 pixel-by-pixel로 비교했습니다.

> **Artifact note:** 현재 제공된 `design_1_wrapper.xsa`에서 `FCLK_CLK0`, AXI DMA, Sobel IP clock은 **50 MHz**로 구성되어 있습니다. 따라서 이 README의 throughput 계산은 50 MHz build를 기준으로 합니다. 별도의 100 MHz implementation report가 확인되면 해당 결과를 별도 표기합니다.

## Project Highlights

- Zynq PS ↔ DDR ↔ AXI DMA ↔ PL 데이터 경로 구성
- 32-bit AXI4-Stream 기반 custom Sobel RTL IP
- 2-line buffer + horizontal shift registers 기반 3×3 sliding window
- `|Gx| + |Gy|` 기반 gradient magnitude approximation
- AXI4-Stream backpressure를 고려한 `TVALID/TREADY` handshake
- window가 완성되지 않는 첫 2행/2열에서 0을 출력하여 input/output stream length 유지
- stall이 없는 steady state에서 **1 accepted pixel / cycle**
- Python reference와 FPGA output의 pixel-by-pixel comparison flow
- Vivado/Vitis 기반 Zynq hardware/software integration

---

## 1. System Architecture

![Zybo Z7-10 Sobel system](https://velog.velcdn.com/images/vom/post/0d167f41-6610-4259-8648-cb02060fd64a/image.jpg)

```text
Input image
    ↓
Zynq PS
    ↓
DDR
    ↓ AXI DMA MM2S
AXI4-Stream
    ↓
Sobel RTL IP
    ↓
AXI4-Stream
    ↓ AXI DMA S2MM
DDR
    ↓
Zynq PS
    ↓ UART
PC / output image
```

![System architecture](https://velog.velcdn.com/images/vom/post/eec8feff-a40b-42e8-8d8e-6940deb953b9/image.png)

The Sobel calculation is implemented in programmable logic. The PS is responsible for preparing image data in DDR, controlling the DMA transfer, and exporting the processed result.

### Verified hardware configuration from XSA

| Item | Configuration |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| Vivado | 2025.2 |
| Sobel IP clock | 50 MHz |
| AXI DMA mode | Simple DMA, scatter-gather disabled |
| MM2S stream width | 32 bit |
| S2MM stream width | 32 bit |
| Sobel `DATA_WIDTH` | 32 bit |
| Sobel `IMAGE_WIDTH` | 640 |

---

## 2. Development Environment

| Item | Specification |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| HDL | Verilog |
| FPGA Tool | Vivado 2025.2 |
| Software Tool | Vitis |
| Processor | Zynq-7000 Processing System |
| Streaming Interface | AXI4-Stream |
| Data Transfer | AXI DMA |
| Pixel Format | 8-bit grayscale |
| Resolution | 640 × 480 |
| Verified PL clock | 50 MHz |
| Functional Reference | Python / NumPy |

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

The core does not buffer the entire frame. Two previous rows are retained in line buffers and combined with horizontal shift registers to generate a new 3×3 neighborhood for each accepted input pixel.

### 3.1 AXI4-Stream handshake

The core uses a one-entry registered output stage.

```text
s_axis_tready = !m_axis_tvalid_reg || m_axis_tready
```

When downstream logic deasserts `m_axis_tready`, the input side also stops accepting new beats and the line buffers, window registers, and x/y counters hold their state. `TLAST` is propagated with the corresponding accepted input beat.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/051a1a69-3726-40a4-8353-5918dbbef720/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/e6d660f8-5578-45a4-a360-e8725dd9c72f/image.jpg" width="49%">
</p>
<p>
  <img src="https://velog.velcdn.com/images/vom/post/2c536d86-40d5-4e9e-b002-790235caae56/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/3d4709fc-8b76-42bb-bff1-09fd7e51cd6f/image.jpg" width="49%">
</p>

### 3.2 Line Buffer & Sliding Window

```text
row y-2 ── line buffer ──┐
row y-1 ── line buffer ──┼── 3×3 window
row y   ── current pixel ┘
```

The implemented convolution uses the current pixel as the bottom-right point of the effective 3×3 window. For `x < 2` or `y < 2`, a complete window is not yet available, so the core outputs zero while preserving the number of AXI stream beats.

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

Instead of implementing

```text
sqrt(Gx^2 + Gy^2)
```

the RTL uses

```text
magnitude = |Gx| + |Gy|
```

and saturates values above 255 to `8'hFF`. Since the Sobel coefficients are limited to `-2, -1, 0, 1, 2`, the datapath is expressed using add/subtract and shifts rather than general-purpose multiplication.

---

## 4. Throughput

The AXI stream datapath can accept one pixel beat every cycle when `TVALID` and `TREADY` remain asserted continuously.

For a 640×480 frame:

```text
640 × 480 = 307,200 pixel beats
```

With the **50 MHz clock verified in the provided XSA**:

```text
307,200 cycles / 50,000,000 cycles/s
= 6.144 ms
```

| Metric | Value |
|---|---:|
| Resolution | 640 × 480 |
| Pixels per frame | 307,200 |
| Verified core clock | 50 MHz |
| Steady-state throughput | 1 accepted pixel / cycle |
| Ideal stream time | 6.144 ms / frame |

> This is the ideal Sobel-core stream time. It excludes DMA setup, DDR access variation, PS software overhead, stalls, and UART transfer time.

---

## 5. Functional Verification

The test image is converted to grayscale, resized to 640×480, and converted to an 8-bit pixel array in Python. The reference model reproduces the RTL's effective 3×3 window alignment, `|Gx| + |Gy|` operation, 8-bit saturation, and first-two-row/column zero behavior.

| Python Reference | FPGA Output |
|---|---|
| ![Python reference](https://velog.velcdn.com/images/vom/post/ddf5f546-1725-4d1b-a493-322bdaab7879/image.png) | ![FPGA output](https://velog.velcdn.com/images/vom/post/f9004dee-1a3b-47db-af3e-9ab7005b6525/image.png) |

![Difference check](https://velog.velcdn.com/images/vom/post/d22e4336-0e3f-42b3-bd4d-486c2630d65b/image.png)

Reported comparison result for the test image:

```text
max error      : 0
mismatch count : 0
mismatch ratio : 0%
```

---

## 6. Implementation Results

The following values were recorded from the Vivado implementation result supplied with the project notes:

| Item | Reported Result |
|---|---:|
| WNS | 1.702 ns |
| TNS | 0 ns |
| LUT | 6,659 / 17,600 (37.84%) |
| FF | 13,990 / 35,200 (39.74%) |
| BRAM | 2 / 60 (3.33%) |
| On-Chip Power | 1.535 W |

![Vivado implementation result](https://velog.velcdn.com/images/vom/post/15c43b43-1508-4b1a-b43f-9690c00294e2/image.png)

The clock associated with these report values must be kept consistent with the implementation artifact. The XSA currently committed/provided for review is a **50 MHz** hardware build; a separate timing report is required before labeling the same WNS as a 100 MHz result.

---

## 7. Current Review Point: Pixel Packing

The provided hardware platform uses a **32-bit AXI4-Stream** for both DMA directions, while the Sobel RTL consumes only `s_axis_tdata[7:0]` as the grayscale pixel and returns the processed pixel in the low byte of a 32-bit word.

Therefore the PS software must define the exact memory representation and DMA transfer length consistently. The Vitis application source is required to verify whether one 8-bit pixel is expanded to one 32-bit DMA word and how the output words are converted back to a byte image.

This interface detail is intentionally not claimed as verified until the PS-side source is added.

---

## 8. Repository Structure

```text
Sobel-Edge-Detection-RTL/
├── README.md
├── rtl/
│   └── sobel_axis_core.v
├── software/            # Vitis DMA/DDR/UART application source (to add)
├── python/
│   ├── png_to_header.py
│   ├── imgtest.py
│   └── capture_uart_image.py
├── constraints/         # Custom XDC if used
├── docs/
└── assets/              # Local copies of diagrams/reports/result images
```

Generated Vivado run/cache directories and generated image headers should not be committed unless specifically needed for reproduction.

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
AXI DMA / Zynq PS integration
    ↓
Python reference verification
    ↓
Implementation / timing / resource analysis
```

The project is intended to demonstrate **algorithm-to-RTL conversion and streaming FPGA datapath design**, not merely the Sobel equation itself.
