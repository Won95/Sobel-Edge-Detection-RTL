# Portfolio Completion Checklist

현재 README에 반영된 시스템 설명과 측정값을 기준으로, 이 저장소를 실제 취업 포트폴리오로 완성하기 위해 남은 자료를 정리합니다.

## 1. Confirmed Project Facts

README에 반영 완료된 내용입니다.

- Zybo Z7-10 / `xc7z010clg400-1`
- Verilog RTL
- Vivado / Vitis
- Zynq PS + DDR + AXI DMA + AXI4-Stream
- 8-bit grayscale, 640×480
- 2-line-buffer 기반 3×3 sliding window
- Sobel `Gx`, `Gy`, `|Gx| + |Gy|`
- 1 pixel/cycle steady-state streaming architecture
- Python reference model과 pixel-by-pixel 비교
- max error = 0
- mismatch count = 0
- WNS = 1.702 ns
- TNS = 0 ns
- LUT = 6,659 / 17,600
- FF = 13,990 / 35,200
- BRAM = 2 / 60
- On-Chip Power = 1.535 W

## 2. Files to Upload Next

우선순위 순서입니다.

### RTL

`rtl/`

- Sobel top module
- AXI4-Stream wrapper/interface logic
- line buffer module
- 3×3 window generator
- Gx/Gy convolution logic
- magnitude / saturation / border handling logic
- 기타 직접 작성한 synthesizable RTL

### Constraints

`constraints/`

- 사용한 XDC
- 100 MHz clock constraint를 확인할 수 있는 파일

### PS Software

`software/`

- AXI DMA 초기화 코드
- MM2S / S2MM transfer 코드
- DDR input/output buffer 처리
- UART로 결과를 전달한 코드

자동 생성된 BSP 전체를 넣기보다 직접 작성한 application source 중심으로 정리합니다.

### Python Reference

`python/`

- 원본 이미지 grayscale 변환
- 640×480 resize
- Sobel reference implementation
- FPGA output과 reference 간 difference check
- mismatch count / max error 계산

이 부분은 RTL 기능 검증 근거이므로 포트폴리오 가치가 높습니다.

## 3. Assets

현재 README의 이미지는 Velog CDN을 직접 참조하고 있습니다. 장기적으로는 아래 이름으로 repository 내부 `assets/`에 보관하는 것이 좋습니다.

```text
assets/
├── zybo_system.jpg
├── system_architecture.png
├── axi_stream_01.jpg
├── axi_stream_02.jpg
├── axi_stream_03.jpg
├── axi_stream_04.jpg
├── line_buffer_01.jpg
├── line_buffer_02.jpg
├── sobel_datapath_01.jpg
├── sobel_datapath_02.jpg
├── sobel_datapath_03.jpg
├── sobel_datapath_04.jpg
├── sobel_datapath_05.jpg
├── input_original.png
├── input_640x480.png
├── python_reference.png
├── fpga_output.png
├── difference_check.png
└── vivado_implementation.png
```

외부 블로그 이미지 링크가 깨져도 포트폴리오가 유지되도록 최종적으로 repository 자체 asset으로 바꾸는 것이 목적입니다.

## 4. Verification Evidence Worth Adding

가능하면 다음 자료를 추가합니다.

- AXI4-Stream `TVALID/TREADY/TDATA/TLAST` simulation waveform
- 첫 유효 Sobel output이 나오는 cycle을 보여주는 waveform
- border padding 동작 확인 waveform
- Python comparison script 실행 결과
- Vivado timing summary 원본 screenshot/report
- Vivado utilization report

## 5. Points to Verify from Source Code

README를 코드와 1:1로 맞추기 위해 source upload 후 아래를 다시 확인합니다.

- AXI stream data width
- 8-bit pixel packing/unpacking 방식
- `TLAST` 생성/전달 방식
- backpressure 발생 시 line-buffer/window state가 정확히 stall되는지
- border padding의 정확한 규칙
- gradient magnitude saturation/clipping 방식
- line buffer가 register array인지 BRAM inference 구조인지
- 실제 pipeline stage 수와 initial latency
- early multi-pixel architecture의 실제 자원 초과 report가 남아 있는지

확인 전에는 README에 구현 세부사항을 추정해서 추가하지 않습니다.

## 6. Portfolio Narrative

이 프로젝트에서 강조할 핵심은 단순히 `Sobel filter를 구현했다`가 아닙니다.

```text
Algorithm
  -> streaming datapath architecture
  -> line-buffer based data reuse
  -> AXI4-Stream protocol integration
  -> DMA-based PS/PL system integration
  -> resource/parallelism trade-off
  -> Python reference verification
  -> implementation and timing closure
```

FPGA Algorithm / RTL / Digital Design 직무에서는 이 흐름이 가장 중요한 설명 축입니다.

## 7. Do Not Upload

- Vivado `.cache/`
- Vivado generated `.runs/` 전체
- `.Xil/`
- machine-specific temporary files
- 개인 absolute path가 포함된 logs
- 자동 생성된 대용량 BSP 전체
- 라이선스가 불명확한 외부 IP source

필요한 것은 프로젝트 백업이 아니라 **직접 설계한 부분과 그 설계가 동작한다는 검증 근거**입니다.
