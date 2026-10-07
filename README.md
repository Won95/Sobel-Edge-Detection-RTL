# Zynq FPGA 기반 AXI DMA Sobel Edge Detection

Zybo Z7-10의 **Zynq PS + AXI DMA + AXI4-Stream + Custom Sobel RTL IP**를 이용해 구현한 FPGA 이미지 처리 프로젝트입니다.

640×480 8-bit grayscale 이미지를 DDR에서 AXI DMA를 통해 PL로 전달하고, Sobel IP에서 **2개의 Line Buffer를 이용한 3×3 Sliding Window**와 Sobel Convolution을 수행한 뒤 결과를 다시 DDR로 반환합니다. FPGA 출력은 RTL과 동일한 연산 및 경계 처리 방식을 구현한 Python Reference Model과 픽셀 단위로 비교하여 검증했습니다.

> **현재 확인된 구현 기준:** 제공된 `design_1_wrapper.xsa`에서 `FCLK_CLK0`, AXI DMA, Sobel IP의 Clock은 **50 MHz**입니다. 따라서 아래 처리시간 계산은 50 MHz Build를 기준으로 작성했습니다.

## 프로젝트 핵심 내용

- Zynq PS ↔ DDR ↔ AXI DMA ↔ PL 데이터 경로 구성
- 32-bit AXI4-Stream 기반 Custom Sobel RTL IP 구현
- 8-bit grayscale pixel을 **32-bit AXI Word의 LSB에 1 pixel/word로 매핑**
- 2-Line Buffer + Shift Register 기반 3×3 Sliding Window 구현
- `|Gx| + |Gy|` 기반 Gradient Magnitude 근사
- `TVALID/TREADY`를 이용한 AXI4-Stream Backpressure 처리
- 3×3 Window가 완성되지 않는 첫 2행/2열에서 0을 출력해 전체 Stream 길이 유지
- Stall이 없는 정상 동작 구간에서 **1 pixel / cycle 처리**
- Python Reference와 FPGA Output의 Pixel-by-Pixel 비교
- Vivado / Vitis를 이용한 Zynq HW/SW 통합

---

## 1. 시스템 구조

![Zybo Z7-10 Sobel system](https://velog.velcdn.com/images/vom/post/0d167f41-6610-4259-8648-cb02060fd64a/image.jpg)

```text
8-bit Grayscale Image
        ↓
Python -> image_data.h
        ↓
Zynq PS
        ↓  1 pixel -> 32-bit word (0x000000PP)
DDR TX Buffer
        ↓ AXI DMA MM2S
32-bit AXI4-Stream
        ↓
Sobel RTL IP
        ↓
32-bit AXI4-Stream
        ↓ AXI DMA S2MM
DDR RX Buffer
        ↓  LSB 1 byte 추출
Zynq PS
        ↓ UART (1 byte / pixel)
PC -> Output PNG
```

![System architecture](https://velog.velcdn.com/images/vom/post/eec8feff-a40b-42e8-8d8e-6940deb953b9/image.png)

### 확인된 하드웨어 설정

| 항목 | 설정 |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| Vivado | 2025.2 |
| Sobel / DMA Clock | 50 MHz |
| AXI DMA Mode | Simple DMA, SG Disabled |
| MM2S Stream Width | 32 bit |
| S2MM Stream Width | 32 bit |
| DMA Length Width | 23 bit |
| Sobel `DATA_WIDTH` | 32 bit |
| Sobel `IMAGE_WIDTH` | 640 |

---

## 2. PS 측 Pixel Packing 및 DMA 전송

입력 이미지는 640×480 해상도의 8-bit grayscale 이미지로 총 픽셀 수는 다음과 같습니다.

```text
640 × 480 = 307,200 pixels
```

AXI DMA의 Stream Width가 32-bit이므로 Vitis 프로그램에서 각 8-bit pixel을 하나의 32-bit word로 확장합니다.

```c
TxBufferPtr[i] = (u32)image_data[i];
```

메모리 및 Stream 상의 데이터 형태는 다음과 같습니다.

```text
0x000000PP
```

따라서 한 Frame의 DMA 전송량은 다음과 같습니다.

```text
307,200 pixels
× 4 bytes / AXI word
= 1,228,800 bytes
```

TX와 RX 각각 1,228,800 byte를 전송합니다.

DMA 전송 시에는 Sobel IP가 출력 데이터를 발생시키기 전에 수신 경로가 준비되도록 **S2MM(RX)을 먼저 설정한 후 MM2S(TX)를 시작**합니다.

또한 DMA 전송 전에는 Cache Flush를 수행하고, 수신 완료 후에는 RX Buffer에 대해 Cache Invalidate를 수행합니다.

DMA 처리가 끝난 뒤에는 32-bit 결과 word의 LSB 1 byte만 추출하여 UART로 전송합니다.

```c
outbyte((char)(RxBufferPtr[i] & 0xFFU));
```

PC 측 Python 프로그램은 UART로 `width × height`개의 byte를 수신한 뒤 8-bit grayscale PNG로 복원합니다.

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

전체 Frame을 PL 내부에 저장하지 않고, 이전 두 행의 pixel만 Line Buffer에 유지하면서 현재 입력 pixel과 결합해 3×3 Window를 생성합니다.

### 3-1. AXI4-Stream Flow Control

Sobel IP는 다음 조건으로 입력 `TREADY`를 생성합니다.

```text
s_axis_tready = !m_axis_tvalid_reg || m_axis_tready
```

Downstream에서 `m_axis_tready`가 0이 되어 Backpressure가 발생하면 새로운 입력 beat를 받지 않고 Line Buffer, Window Register, Pixel Counter의 상태를 유지합니다.

`TLAST` 역시 해당 입력 beat와 함께 출력으로 전달합니다.

### 3-2. Line Buffer & Sliding Window

```text
row y-2 ── Line Buffer ──┐
row y-1 ── Line Buffer ──┼── 3×3 Window
row y   ── Current Pixel ┘
```

현재 pixel은 유효한 3×3 Window의 오른쪽 아래 pixel에 해당합니다.

따라서 `x < 2` 또는 `y < 2` 구간에서는 아직 3×3 Window가 완성되지 않으므로 결과값으로 0을 출력합니다. 이를 통해 입력 pixel 수와 출력 pixel 수를 동일하게 유지합니다.

<p>
  <img src="https://velog.velcdn.com/images/vom/post/02839836-1281-4437-99a9-ee11128f04ac/image.jpg" width="49%">
  <img src="https://velog.velcdn.com/images/vom/post/040a22d3-8f28-4e1c-98d3-8716f31806d9/image.jpg" width="49%">
</p>

### 3-3. Sobel Convolution

Sobel Operator의 Gx, Gy Kernel은 다음과 같습니다.

```text
Gx = [ -1   0   1 ]      Gy = [ -1  -2  -1 ]
     [ -2   0   2 ]           [  0   0   0 ]
     [ -1   0   1 ]           [  1   2   1 ]
```

일반적인 Gradient Magnitude 계산식인

```text
sqrt(Gx^2 + Gy^2)
```

대신 FPGA 구현에서는 연산량을 줄이기 위해 다음 근사식을 사용했습니다.

```text
magnitude = |Gx| + |Gy|
```

계산 결과가 255를 초과하면 `8'hFF`로 Saturation합니다.

Sobel 계수가 `-2, -1, 0, 1, 2`로 제한되기 때문에 일반적인 Multiplier를 사용하지 않고 Shift 및 Add/Subtract 연산으로 구현했습니다.

---

## 4. Throughput 및 처리시간

Backpressure 없이 `TVALID/TREADY` Handshake가 매 Clock마다 연속으로 성립하는 경우, Sobel Datapath는 **1 cycle에 1 pixel**을 처리할 수 있습니다.

640×480 Frame은 총 307,200개의 pixel로 구성됩니다.

현재 확인된 50 MHz Build 기준 이론적인 Core Stream 처리시간은 다음과 같습니다.

```text
307,200 cycles / 50,000,000 cycles/s
= 6.144 ms
```

| 항목 | 값 |
|---|---:|
| Resolution | 640 × 480 |
| Pixels / Frame | 307,200 |
| 확인된 Core Clock | 50 MHz |
| Steady-State Throughput | 1 pixel / cycle |
| 이상적 Core Stream Time | 6.144 ms / frame |
| DMA Transfer Size | 1,228,800 bytes / direction |

> `6.144 ms`는 Sobel Core 내부의 이상적인 Streaming 처리시간입니다. DMA 설정 시간, DDR Access, PS Software Overhead, AXI Stall, UART 전송 시간은 포함하지 않습니다.

---

## 5. 기능 검증

Python 기반 검증 흐름은 다음과 같습니다.

```text
Input PNG
  ↓ Grayscale 변환
  ↓ 640×480 Resize
  ↓ C Header 생성
FPGA Processing
  ↓ UART Capture
FPGA Result PNG
  ↓
Python Sobel Reference
  ↓
Pixel-by-Pixel Difference Check
```

Python Reference Model은 RTL과 동일하게 다음 동작을 재현합니다.

- 3×3 Window Alignment
- `|Gx| + |Gy|`
- 8-bit Saturation
- 첫 2행/2열에서 0 출력

| Python Reference | FPGA Output |
|---|---|
| ![Python reference](https://velog.velcdn.com/images/vom/post/ddf5f546-1725-4d1b-a493-322bdaab7879/image.png) | ![FPGA output](https://velog.velcdn.com/images/vom/post/f9004dee-1a3b-47db-af3e-9ab7005b6525/image.png) |

![Difference check](https://velog.velcdn.com/images/vom/post/d22e4336-0e3f-42b3-bd4d-486c2630d65b/image.png)

테스트 이미지 기준 비교 결과는 다음과 같습니다.

```text
max error      : 0
mismatch count : 0
mismatch ratio : 0%
```

즉, 해당 테스트 이미지에서 FPGA 출력과 Python Reference 결과가 Pixel 단위로 일치함을 확인했습니다.

---

## 6. Implementation 결과

현재 프로젝트 기록에 남아 있는 Vivado Implementation 결과는 다음과 같습니다.

| 항목 | 결과 |
|---|---:|
| WNS | 1.702 ns |
| TNS | 0 ns |
| LUT | 6,659 / 17,600 (37.84%) |
| FF | 13,990 / 35,200 (39.74%) |
| BRAM | 2 / 60 (3.33%) |
| On-Chip Power | 1.535 W |

![Vivado implementation result](https://velog.velcdn.com/images/vom/post/15c43b43-1508-4b1a-b43f-9690c00294e2/image.png)

현재 확인된 XSA는 50 MHz Build입니다. 따라서 위 WNS 및 Utilization 수치가 어떤 Clock Constraint에서 나온 결과인지 최종적으로 확정하려면 원본 `timing_summary.rpt`, `utilization.rpt` 확인이 필요합니다.

---

## 7. 현재 구조의 제한 및 개선 가능점

현재 RTL은 **Reset 이후 1개의 Frame을 처리하는 구조**를 기준으로 구현되어 있습니다.

`TLAST`는 DMA 방향으로 전달되지만 Frame 종료 시 내부 `y_cnt`와 Line Buffer History를 초기화하지 않습니다. 따라서 Reset 없이 여러 Frame을 연속 처리하려면 Frame Boundary에서 내부 상태를 초기화하는 로직이 추가로 필요합니다.

또한 현재 Line Buffer는 배열 전체를 Reset하는 구조이므로 BRAM Inference에 불리할 가능성이 있습니다.

향후 개선 방향은 다음과 같습니다.

- Multi-Frame Continuous Streaming 지원
- Frame 종료 시 내부 State 초기화
- BRAM 기반 Line Buffer 구조로 변경
- Sobel Arithmetic Pipeline 분할
- Resource / Timing 비교 분석

---

## 8. Repository 구조

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

Vivado가 자동 생성하는 Cache, Run Directory, Bitstream, XSA와 Python에서 생성 가능한 `image_data.h`는 포트폴리오 소스에서 제외합니다.

---

## 9. 이 프로젝트에서 보여주고자 한 내용

```text
Image Processing Algorithm
        ↓
Streaming Hardware Datapath
        ↓
Line Buffer 기반 Data Reuse
        ↓
AXI4-Stream Flow Control
        ↓
32-bit DMA / 8-bit Pixel Interface Mapping
        ↓
Zynq PS-PL Integration
        ↓
Python Reference Verification
        ↓
Implementation / Timing / Resource Analysis
```

단순히 Sobel 수식을 RTL로 구현하는 데 그치지 않고, **이미지 처리 알고리즘을 Streaming Datapath로 구조화하고 AXI DMA를 이용해 실제 Zynq 시스템에서 처리 및 검증하는 전체 과정**을 구현한 프로젝트입니다.
