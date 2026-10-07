# Sobel Edge Detection on FPGA using AXI DMA & AXI4-Stream

Zybo Z7-10의 **Zynq PS + AXI DMA + AXI4-Stream + custom Sobel RTL IP**를 이용해 구현한 FPGA 이미지 처리 프로젝트입니다.

DDR에 저장된 640×480 grayscale 이미지를 AXI DMA로 PL에 전달하고, PL의 Sobel IP가 **3×3 sliding window 기반 edge detection**을 수행한 뒤 처리 결과를 다시 DDR로 반환합니다. RTL 결과는 Python reference model과 비교하여 **pixel-by-pixel mismatch 0**으로 검증했습니다.

## Project Highlights

- Zynq PS ↔ DDR ↔ AXI DMA ↔ PL 데이터 경로 구성
- AXI4-Stream 기반 custom Sobel RTL IP 설계
- 2-line buffer + shift register 기반 3×3 sliding window 구현
- `|Gx| + |Gy|` 근사를 이용한 gradient magnitude 계산
- 출력 stream 길이를 유지하기 위한 border padding 처리
- Pipeline fill 이후 **1 pixel / cycle** 처리 구조
- Python reference 대비 **max error = 0, mismatch = 0**
- 100 MHz implementation 기준 **WNS = 1.702 ns, TNS = 0 ns**

---

## 1. System Overview

![Zybo Z7-10 Sobel system](https://velog.velcdn.com/images/vom/post/0d167f41-6610-4259-8648-cb02060fd64a/image.jpg)

### Data Flow

```text
Image header / input data
        |
        v
Zynq PS
        |
        v
DDR Memory
        |
        |  AXI DMA MM2S
        v
AXI4-Stream
        |
        v
Custom Sobel IP
        |
        v
AXI4-Stream
        |
        |  AXI DMA S2MM
        v
DDR Memory
        |
        v
Zynq PS
        |
        |  UART
        v
PC -> output image
```

The PS initializes the input image in DDR, configures the DMA transfer, and reads the processed result after the S2MM transfer completes. The Sobel calculation itself is performed in programmable logic as a streaming RTL datapath.

![System architecture](https://velog.velcdn.com/images/vom/post/eec8feff-a40b-42e8-8d8e-6940deb953b9/image.png)

---

## 2. Development Environment

| Item | Specification |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| HDL | Verilog |
| FPGA Tool | Vivado |
| Software Tool | Vitis |
| Processor | Zynq-7000 Processing System |
| Streaming Interface | AXI4-Stream |
| Data Transfer | AXI DMA |
| Pixel Format | 8-bit grayscale |
| Resolution | 640 × 480 |
| Target Clock | 100 MHz |
| Functional Reference | Python reference model |

---

## 3. Sobel IP Architecture

The Sobel IP is organized as a streaming datapath:

```text
AXI4-Stream Input
        |
        v
Line Buffer / 3x3 Window Generator
        |
        v
Sobel Gx / Gy Convolution
        |
        v
Absolute Value / Magnitude Approximation
        |
        v
Border Handling / Output Formatting
        |
        v
AXI4-Stream Output
```

The design does not store an entire frame inside the Sobel core. Instead, it reuses recently received pixels through line buffers and shift registers to continuously generate a 3×3 neighborhood.

### 3.1 AXI4-Stream Interface

The custom IP is directly connected between the MM2S and S2MM channels of AXI DMA through AXI4-Stream.

The stream protocol allows the image datapath to operate independently of DDR transactions while `TVALID/TREADY` handshake signals control actual data movement. Border pixels are padded so that the **number of output pixels remains equal to the number of input pixels**, which keeps the DMA transfer length consistent.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/051a1a69-3726-40a4-8353-5918dbbef720/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/e6d660f8-5578-45a4-a360-e8725dd9c72f/image.jpg" width="49%">
</p>
<p>
  <img src="https://velog.velcdn.com/images/vom/post/2c536d86-40d5-4e9e-b002-790235caae56/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/3d4709fc-8b76-42bb-bff1-09fd7e51cd6f/image.jpg" width="49%">
</p>

### 3.2 Line Buffer & Sliding Window

A 3×3 convolution requires pixels from the current row and the previous two rows. The design therefore uses **two line buffers plus shift registers** to reuse incoming pixel data and form a sliding 3×3 window.

```text
Previous row -2  ---- Line Buffer ----+--- 3 pixels
                                     |
Previous row -1  ---- Line Buffer ----+--- 3 pixels ---> 3x3 Window
                                     |
Current row       ---- Shift Reg ------+--- 3 pixels
```

After the initial window-fill latency, a new window can be generated for each accepted input pixel.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/02839836-1281-4437-99a9-ee11128f04ac/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/040a22d3-8f28-4e1c-98d3-8716f31806d9/image.jpg" width="49%">
</p>

### 3.3 Sobel Convolution

The Sobel operator calculates horizontal and vertical gradients using the following kernels.

```text
Gx = [ -1   0   1 ]      Gy = [ -1  -2  -1 ]
     [ -2   0   2 ]           [  0   0   0 ]
     [ -1   0   1 ]           [  1   2   1 ]
```

Instead of calculating the Euclidean magnitude

```text
sqrt(Gx^2 + Gy^2)
```

the RTL uses the hardware-friendly approximation

```text
magnitude = |Gx| + |Gy|
```

This avoids square, addition of squares, and square-root hardware while preserving the edge-strength information needed for this application.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/a4f4a63d-29a3-42a4-855e-cbcdaa9d094d/image.jpg" width="32%">
  <img src="https://velog.velcdn.com/images/vom/post/34426e7b-e145-436b-ab6d-28cdb435c266/image.jpg" width="32%">
  <img src="https://velog.velcdn.com/images/vom/post/01199a0e-caaa-4cdf-9b51-63ad757a83b0/image.jpg" width="32%">
</p>
<p>
  <img src="https://velog.velcdn.com/images/vom/post/d5770f77-81b9-4481-9c9b-ec05e71c6a1f/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/0b66784e-607e-440f-950e-a4731e5e51c0/image.jpg" width="49%">
</p>

---

## 4. Throughput and Latency

The final Sobel datapath is designed for a throughput of **one accepted pixel per clock cycle** after the pipeline/window-fill latency.

### Theoretical Core Throughput

```text
1 pixel / cycle
```

For a 640×480 image:

```text
640 × 480 = 307,200 pixels
```

At a 100 MHz clock, the ideal streaming time for 307,200 pixel transfers is:

```text
307,200 cycles / 100,000,000 cycles/s
= 3.072 ms
```

| Metric | Value |
|---|---:|
| Resolution | 640 × 480 |
| Pixels per frame | 307,200 |
| Target clock | 100 MHz |
| Steady-state throughput | 1 pixel / cycle |
| Ideal core stream time | 3.072 ms / frame |

> **Note:** `3.072 ms` is a theoretical Sobel-core streaming value assuming continuous `TVALID/TREADY` handshakes with no backpressure. It does **not** include PS software overhead, DDR/DMA setup latency, UART transfer time, or other end-to-end system overhead.

---

## 5. Design Trade-off: Parallelism vs FPGA Resources

An early architecture attempted to process multiple pixels in parallel. Expanding the sliding-window and Sobel arithmetic for multiple simultaneous pixels increased the amount of duplicated combinational logic and LUT usage until the design exceeded the available resources of the target device.

The architecture was therefore redesigned around a **1 pixel/cycle streaming pipeline**.

```text
More pixel-level parallelism
        |
        +--> Higher instantaneous computation
        |
        +--> Duplicated window / arithmetic logic
        |
        +--> Higher LUT / FF utilization
        |
        v
Resource limit on xc7z010

            ↓ redesign

1 pixel/cycle streaming architecture
        |
        +--> Continuous pipeline
        +--> Lower resource pressure
        +--> 100 MHz timing closure
```

This was a deliberate architecture trade-off: rather than maximizing spatial parallelism, the final design targets a sustainable streaming throughput that fits the Zybo Z7-10 device.

---

## 6. Functional Verification

The input image was converted to grayscale and resized to **640×480** in Python before being used by the FPGA system.

### 6.1 Input Image

| Original | 640×480 Input |
|---|---|
| ![Original image](https://velog.velcdn.com/images/vom/post/84273bf8-9e53-4586-81df-ce3cf05c5eb7/image.png) | ![Resized input](https://velog.velcdn.com/images/vom/post/b23a563f-e05d-4632-8a19-118f2777fbee/image.png) |

### 6.2 Python Reference vs FPGA Output

The same Sobel algorithm and border behavior were implemented in Python and used as a reference model.

| Python Reference | FPGA Output |
|---|---|
| ![Python reference](https://velog.velcdn.com/images/vom/post/ddf5f546-1725-4d1b-a493-322bdaab7879/image.png) | ![FPGA output](https://velog.velcdn.com/images/vom/post/f9004dee-1a3b-47db-af3e-9ab7005b6525/image.png) |

### 6.3 Pixel-by-Pixel Difference Check

![Difference check](https://velog.velcdn.com/images/vom/post/d22e4336-0e3f-42b3-bd4d-486c2630d65b/image.png)

```text
max error      : 0
mismatch count : 0
mismatch ratio : 0%
```

The FPGA output matched the Python reference for every compared pixel in the test image.

---

## 7. Implementation Results

The design was synthesized and implemented in Vivado for `xc7z010clg400-1` at a 100 MHz target clock.

![Vivado implementation result](https://velog.velcdn.com/images/vom/post/15c43b43-1508-4b1a-b43f-9690c00294e2/image.png)

| Item | Result |
|---|---:|
| WNS | **1.702 ns** |
| TNS | **0 ns** |
| LUT | 6,659 / 17,600 (**37.84%**) |
| FF | 13,990 / 35,200 (**39.74%**) |
| BRAM | 2 / 60 (**3.33%**) |
| On-Chip Power | **1.535 W** |

A positive WNS and zero TNS indicate that the constrained 100 MHz design closed timing in the reported implementation run.

---

## 8. What This Project Demonstrates

This project focuses on the process of turning an image-processing algorithm into an FPGA streaming architecture rather than only implementing the Sobel equation itself.

```text
Image-processing algorithm
        ↓
3x3 neighborhood requirement
        ↓
Line-buffer / sliding-window architecture
        ↓
Streaming Sobel datapath
        ↓
AXI4-Stream integration
        ↓
AXI DMA + Zynq PS system integration
        ↓
Python reference verification
        ↓
Synthesis / implementation / timing closure
```

Key engineering topics demonstrated in the project are:

- algorithm-to-datapath conversion
- streaming image processing
- line-buffer based data reuse
- sliding-window generation
- AXI4-Stream handshake integration
- AXI DMA based PS–PL data movement
- resource/parallelism trade-off
- Python golden/reference model based RTL verification
- Vivado synthesis, implementation, utilization, and timing analysis

---

## 9. Repository Structure

The repository will be organized around source files required to reproduce and review the design rather than the entire generated Vivado project directory.

```text
Sobel-Edge-Detection-RTL/
├── README.md
├── rtl/                 # Synthesizable Verilog RTL
├── constraints/         # XDC constraints
├── software/            # Vitis / PS-side DMA control code
├── python/              # Image preprocessing and reference model
├── docs/                # Architecture and design notes
└── assets/              # Diagrams, result images, screenshots
```

Generated Vivado cache/run directories and machine-specific temporary files are intentionally excluded.
