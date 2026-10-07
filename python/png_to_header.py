from PIL import Image
import numpy as np

INPUT_IMAGE = "testpic.png"
OUTPUT_HEADER = "image_data.h"
WIDTH = 640
HEIGHT = 480

# 1. 이미지 로드
img = Image.open(INPUT_IMAGE).convert("L")
img = img.resize((WIDTH, HEIGHT))

# 2. numpy 배열 변환
arr = np.array(img, dtype=np.uint8)

# 3. 디버그 출력
print("shape:", arr.shape)
print("dtype:", arr.dtype)
print("min:", arr.min())
print("max:", arr.max())
print("mean:", float(arr.mean()))
print("first 32 pixels:", arr.flatten()[:32].tolist())
print("sample pixels:",
      arr[0,0], arr[100,100], arr[200,200], arr[300,300], arr[479,639])

# 4. 헤더 파일 생성
flat = arr.flatten()

with open(OUTPUT_HEADER, "w", encoding="utf-8") as f:
    f.write("#ifndef IMAGE_DATA_H\n")
    f.write("#define IMAGE_DATA_H\n\n")
    f.write(f"#define IMG_WIDTH {WIDTH}\n")
    f.write(f"#define IMG_HEIGHT {HEIGHT}\n\n")
    f.write(f"static const unsigned char image_data[{WIDTH * HEIGHT}] = {{\n")

    for i, val in enumerate(flat):
        if i % 16 == 0:
            f.write("    ")
        f.write(f"{int(val)}")
        if i != len(flat) - 1:
            f.write(", ")
        if i % 16 == 15:
            f.write("\n")

    if len(flat) % 16 != 0:
        f.write("\n")

    f.write("};\n\n")
    f.write("#endif\n")

print(f"Saved {OUTPUT_HEADER}")
