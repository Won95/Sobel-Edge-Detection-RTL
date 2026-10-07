# Portfolio Material Checklist

이 문서는 Sobel FPGA 프로젝트를 취업 포트폴리오 형태로 완성하기 위해 필요한 원본 자료를 정리하는 체크리스트입니다.

## 1. RTL / Project Files

우선순위가 높은 파일부터 업로드합니다.

- Top module
- Sobel calculation module
- Line buffer / sliding-window module
- Pipeline register 관련 module
- Video/HDMI wrapper가 있다면 해당 module
- Testbench
- XDC constraint

Vivado 전체 프로젝트 폴더를 그대로 올리기보다는 직접 작성한 RTL과 constraint 중심으로 정리합니다.

## 2. Images / Screenshots

`assets/`에 다음 자료를 넣습니다.

1. `input_frame.*` - 원본 영상
2. `sobel_output.*` - Sobel 결과
3. `architecture.*` - 전체 datapath/block diagram
4. `waveform.*` - 핵심 simulation waveform
5. `timing_before.*` - pipeline 적용 전 timing 결과
6. `timing_after.*` - pipeline 적용 후 timing 결과
7. `utilization.*` - LUT/FF/BRAM/DSP utilization
8. `fpga_demo.*` - 실제 보드 동작 사진이 있다면 추가

파일명은 가능하면 영문 소문자와 `_`를 사용합니다.

## 3. Timing Data

README에 정확한 수치를 넣기 위해 다음 값을 확보합니다.

- Target clock period / frequency
- Initial WNS / TNS
- Initial critical-path delay
- Final WNS / TNS
- Final achieved clock frequency 또는 timing closure 여부
- Pipeline stage 수

수치를 기억으로 작성하지 않고 Vivado report에서 확인한 값만 사용합니다.

## 4. FPGA Resource Data

Vivado utilization report에서 아래 항목을 확보합니다.

- LUT
- FF
- BRAM
- DSP

가능하면 사용량과 전체 자원 대비 비율을 함께 기록합니다.

## 5. Algorithm / Data Path Explanation

면접에서 설명할 수 있도록 다음 내용을 최종 README에 반영합니다.

- 왜 3x3 sliding window가 필요한가
- line buffer가 왜 필요한가
- Sobel coefficient를 multiplier 없이 구현할 수 있는 이유
- 한 clock당 몇 pixel을 처리하는가
- pipeline 전 critical path가 왜 길었는가
- 어느 위치에 register를 추가했는가
- pipeline으로 latency는 얼마나 증가했고 throughput은 어떻게 유지되는가

## 6. Do Not Upload

다음은 기본적으로 저장소에서 제외합니다.

- Vivado cache
- `.runs/`
- `.cache/`
- 생성된 temporary files
- 불필요한 bitstream 중복 파일
- 개인 경로가 포함된 log
- 라이선스가 불명확한 타인의 IP source

최종 저장소의 목적은 `Vivado 프로젝트 백업`이 아니라 `설계 역량을 보여주는 포트폴리오`입니다.
