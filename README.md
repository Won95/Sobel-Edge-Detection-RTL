# Sobel Edge Detection RTL on FPGA

FPGA 기반 실시간 영상처리 데이터패스 설계 프로젝트입니다. HDMI 입력 영상에 대해 Sobel edge detection을 수행하고, 연산 경로의 timing bottleneck을 분석한 뒤 pipeline을 적용해 timing을 개선하는 과정을 다룹니다.

> Target board: Zybo Z7-10  
> Design focus: FPGA Algorithm / Data Path / Pipelining / Timing Optimization

---

## 1. Project Overview

이 프로젝트의 목적은 단순히 Sobel 알고리즘을 RTL로 옮기는 것이 아니라, 영상처리 알고리즘을 FPGA에서 실시간 처리 가능한 **streaming data path**로 구조화하고, synthesis/timing 결과를 기반으로 병목 경로를 개선하는 것입니다.

### Main Flow

```text
HDMI / Pixel Stream
        |
        v
  Line Buffer
        |
        v
 Sliding 3x3 Window
        |
        v
 Sobel Gx / Gy
        |
        v
 Gradient Calculation
        |
        v
 Threshold / Output
        |
        v
  Video Output
```

---

## 2. Sobel Algorithm

Sobel operator는 3x3 window를 사용해 수평/수직 방향 gradient를 계산합니다.

```text
Gx = [-1  0  1]      Gy = [ 1  2  1]
     [-2  0  2]           [ 0  0  0]
     [-1  0  1]           [-1 -2 -1]
```

RTL에서는 multiplication을 일반 multiplier로 구현하기보다 coefficient가 `-2, -1, 0, 1, 2`라는 점을 이용해 shift/add/subtract 기반 데이터패스로 구성할 수 있습니다.

---

## 3. Hardware Architecture

### 3.1 Streaming Data Path

한 픽셀씩 입력되는 영상 스트림을 처리하기 위해 전체 frame을 저장하지 않고 line buffer와 shift register를 이용해 3x3 window를 생성하는 구조를 사용합니다.

```text
Pixel Input
    |
    +---- Line Buffer 0 ----+
    |                       |
    +---- Line Buffer 1 ----+--> 3x3 Window --> Sobel Core
    |                       |
    +---- Current Line -----+
```

이 구조의 핵심은 **frame memory 기반 처리 대신 streaming 구조를 사용해 한 clock당 한 pixel 처리 가능한 datapath를 구성하는 것**입니다.

### 3.2 Sobel Data Path

```text
3x3 Pixels
    |
    +--> Gx Add/Sub Tree ---+
    |                       |
    +--> Gy Add/Sub Tree ---+--> Gradient --> Threshold --> Edge Pixel
```

---

## 4. Timing Bottleneck & Pipelining

초기 구현에서는 sliding-window 처리와 gradient 연산이 하나의 긴 combinational path에 포함되어 timing violation이 발생했습니다.

초기 timing 분석에서 약 **WNS = -3.5 ns** 수준의 violation이 확인되었으며, 요구 clock period 대비 datapath가 지나치게 길다는 것을 확인했습니다.

### Initial Path

```text
Window Generation
      |
      v
Gx / Gy Calculation
      |
      v
Gradient Calculation
      |
      v
Output
```

### Pipelined Path

```text
Window Generation
      |
     REG
      |
Gx / Gy Calculation
      |
     REG
      |
Gradient / Threshold
      |
     REG
      |
Output
```

Pipeline register를 삽입하여 combinational path를 여러 stage로 분리하고, 각 stage의 logic depth를 줄이는 방식으로 timing을 개선했습니다.

> Final WNS / Fmax: 자료 정리 후 업데이트 예정

---

## 5. Engineering Points

이 프로젝트에서 중점적으로 다룬 내용은 다음과 같습니다.

- Sobel image-processing algorithm의 RTL data path 변환
- 3x3 sliding window 생성
- Line buffer 기반 streaming architecture
- Shift/Add/Subtract 기반 gradient 연산
- Pipeline stage 분할
- Critical path 분석
- Vivado synthesis / implementation timing 분석
- Latency와 throughput의 trade-off

---

## 6. Target FPGA Environment

| Item | Environment |
|---|---|
| FPGA Board | Zybo Z7-10 |
| FPGA | Xilinx Zynq-7000 |
| HDL | Verilog HDL |
| Tool | Xilinx Vivado |
| Application | Real-time Sobel Edge Detection |

세부 Vivado version, clock frequency, video resolution 등은 프로젝트 파일 확인 후 업데이트합니다.

---

## 7. Repository Structure

```text
Sobel-Edge-Detection-RTL/
├── README.md
├── rtl/            # Synthesizable Verilog RTL
├── tb/             # Testbench / simulation files
├── constraints/    # XDC constraints
├── docs/           # Architecture / timing / design notes
└── assets/         # Block diagrams, waveform, Vivado screenshots, result images
```

---

## 8. Results

아래 자료를 추가해 최종 포트폴리오 형태로 정리할 예정입니다.

| Result | Status |
|---|---|
| Original input image/frame | To be added |
| Sobel output image/frame | To be added |
| RTL block diagram | To be added |
| Simulation waveform | To be added |
| Synthesis utilization | To be added |
| Initial timing report | WNS ≈ -3.5 ns |
| Pipelined timing report | To be added |

---

## 9. Portfolio Focus

이 프로젝트는 FPGA를 단순히 HDL 코딩 대상으로 접근하기보다,

**Algorithm → Data Path → Pipeline → Timing Analysis → Optimization**

의 흐름으로 설계한 경험을 보여주는 것을 목표로 합니다.

특히 FPGA Algorithm / RTL / Digital Design 직무에서 다음 역량을 보여줄 수 있도록 저장소를 구성합니다.

- 알고리즘의 hardware architecture 변환 능력
- streaming datapath 설계 경험
- pipeline 기반 timing optimization 경험
- synthesis / implementation 결과 기반 설계 개선 경험
