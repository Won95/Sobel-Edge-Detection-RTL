# Zynq FPGA 기반 Sobel Edge Detection

Zybo Z7-10에서 **AXI DMA + AXI4-Stream + Custom Sobel RTL IP**를 이용해 640×480 grayscale 이미지의 edge detection을 구현한 프로젝트입니다.

Xilinx AXI DMA를 이용해 PS–PL 데이터 전송 구조를 구성하고, PL에는 직접 설계한 Sobel IP를 연결했습니다. FPGA 출력은 Python reference 결과와 pixel 단위로 비교해 검증했습니다.

## 1. 시스템 구조

```text
Input Image
    ↓
Zynq PS / DDR
    ↓ AXI DMA (MM2S)
AXI4-Stream
    ↓
Custom Sobel IP
    ↓
AXI4-Stream
    ↓ AXI DMA (S2MM)
DDR / Zynq PS
    ↓ UART
PC / Output Image
```

![System architecture](https://velog.velcdn.com/images/vom/post/eec8feff-a40b-42e8-8d8e-6940deb953b9/image.png)

| 항목 | 사양 |
|---|---|
| Board | Zybo Z7-10 |
| Device | `xc7z010clg400-1` |
| HDL | Verilog |
| Tool | Vivado / Vitis |
| Interface | AXI4-Stream |
| Data Transfer | AXI DMA |
| Image | 640×480, 8-bit grayscale |
| 확인된 PL Clock | 50 MHz |

## 2. 직접 구현한 부분

### Sobel RTL IP

- AXI4-Stream 입력/출력 인터페이스
- 2개의 Line Buffer를 이용한 3×3 Sliding Window
- Sobel `Gx`, `Gy` 연산
- `|Gx| + |Gy|` 방식의 Gradient Magnitude 계산
- 8-bit Saturation
- `TVALID/TREADY` 기반 Backpressure 처리
- 정상 Streaming 구간에서 **1 pixel / cycle** 처리

```text
AXI4-Stream Input
        ↓
2-Line Buffer
        ↓
3×3 Sliding Window
        ↓
Sobel Gx / Gy
        ↓
|Gx| + |Gy|
        ↓
8-bit Output
```

### PS Software

AXI DMA의 Stream Width가 32-bit이므로 8-bit pixel 하나를 32-bit word의 LSB에 매핑하여 전송했습니다.

```text
8-bit pixel → 0x000000PP → AXI DMA
```

DMA 완료 후 결과 word의 LSB만 추출해 UART로 PC에 전송합니다.

### Python Verification

- 입력 이미지 grayscale 변환 및 640×480 resize
- C header 생성
- Sobel reference model
- FPGA 출력과 pixel-by-pixel 비교

## 3. Sobel 연산

```text
Gx = [ -1   0   1 ]      Gy = [ -1  -2  -1 ]
     [ -2   0   2 ]           [  0   0   0 ]
     [ -1   0   1 ]           [  1   2   1 ]
```

Gradient magnitude는 hardware 연산량을 줄이기 위해 다음과 같이 근사했습니다.

```text
Magnitude = |Gx| + |Gy|
```

3×3 Window가 완성되지 않는 첫 2행/2열에서는 0을 출력하여 입력과 출력의 pixel 수를 동일하게 유지했습니다.

## 4. 검증 결과

| Python Reference | FPGA Output |
|---|---|
| ![Python reference](https://velog.velcdn.com/images/vom/post/ddf5f546-1725-4d1b-a493-322bdaab7879/image.png) | ![FPGA output](https://velog.velcdn.com/images/vom/post/f9004dee-1a3b-47db-af3e-9ab7005b6525/image.png) |

![Difference check](https://velog.velcdn.com/images/vom/post/d22e4336-0e3f-42b3-bd4d-486c2630d65b/image.png)

```text
max error      : 0
mismatch count : 0
mismatch ratio : 0%
```

테스트 이미지에서 Python reference와 FPGA 출력이 pixel 단위로 일치함을 확인했습니다.

## 5. 처리 성능 및 Implementation 결과

640×480 이미지는 총 307,200 pixel이며, 1 pixel/cycle 기준 50 MHz에서 이상적인 Core Streaming 시간은 약 **6.144 ms/frame**입니다.

| 항목 | 결과 |
|---|---:|
| Resolution | 640 × 480 |
| Throughput | 1 pixel / cycle |
| 확인된 PL Clock | 50 MHz |
| Ideal Core Stream Time | 6.144 ms / frame |
| WNS | 1.702 ns |
| TNS | 0 ns |
| LUT | 6,659 / 17,600 (37.84%) |
| FF | 13,990 / 35,200 (39.74%) |
| BRAM | 2 / 60 (3.33%) |
| On-Chip Power | 1.535 W |

![Vivado implementation result](https://velog.velcdn.com/images/vom/post/15c43b43-1508-4b1a-b43f-9690c00294e2/image.png)

## 6. Repository 구조

```text
Sobel-Edge-Detection-RTL/
├── README.md
├── rtl/
│   └── sobel_axis_core.v
├── software/
│   └── main.c
└── python/
    ├── png_to_header.py
    ├── imgtest.py
    └── capture_uart_image.py
```

Vivado 자동 생성 파일과 Python에서 재생성 가능한 `image_data.h`는 저장소에서 제외했습니다.
