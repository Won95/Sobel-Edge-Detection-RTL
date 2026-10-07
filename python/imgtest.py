from PIL import Image
import numpy as np

INPUT_IMAGE = "testpic.png"
FPGA_RESULT = "testpic_result.png"
PY_RESULT   = "test_py_reference.png"

img = Image.open(INPUT_IMAGE).convert("L").resize((640, 480))
img.save("testpic_resize.png")
src = np.array(img, dtype=np.uint8)

h, w = src.shape

ref = np.zeros((h, w), dtype=np.uint8)

for y in range(h):
    for x in range(w):
        if x < 2 or y < 2:
            ref[y, x] = 0
            continue

        # RTL effective window:
        # [w01 w02 top_new] = [src[y-2,x-2] src[y-2,x-1] src[y-2,x]]
        # [w11 w12 mid_new] = [src[y-1,x-2] src[y-1,x-1] src[y-1,x]]
        # [w21 w22 bot_new] = [src[y,  x-2] src[y,  x-1] src[y,  x]]

        w01 = int(src[y-2, x-2])
        w02 = int(src[y-2, x-1])
        top_new = int(src[y-2, x])

        w11 = int(src[y-1, x-2])
        w12 = int(src[y-1, x-1])
        mid_new = int(src[y-1, x])

        w21 = int(src[y, x-2])
        w22 = int(src[y, x-1])
        bot_new = int(src[y, x])

        gx = (
            top_new + (mid_new << 1) + bot_new
            - w01 - (w11 << 1) - w21
        )

        gy = (
            w01 + (w02 << 1) + top_new
            - w21 - (w22 << 1) - bot_new
        )

        mag = abs(gx) + abs(gy)
        if mag > 255:
            mag = 255

        ref[y, x] = mag

Image.fromarray(ref).save(PY_RESULT)
print(f"Saved Python reference: {PY_RESULT}")

fpga = np.array(Image.open(FPGA_RESULT).convert("L"), dtype=np.uint8)

if fpga.shape != ref.shape:
    raise ValueError(f"Shape mismatch: fpga={fpga.shape}, ref={ref.shape}")

diff = np.abs(fpga.astype(np.int16) - ref.astype(np.int16))

max_err = int(diff.max())
mean_err = float(diff.mean())
mismatch = int(np.count_nonzero(diff))
total = diff.size
mismatch_ratio = mismatch / total * 100.0

print("Comparison result")
print(f"  max error       : {max_err}")
print(f"  mean error      : {mean_err:.6f}")
print(f"  mismatch count  : {mismatch}")
print(f"  mismatch ratio  : {mismatch_ratio:.6f}%")

diff_img = np.clip(diff, 0, 255).astype(np.uint8)
Image.fromarray(diff_img).save("diff.png")
print("Saved diff image: diff.png")
