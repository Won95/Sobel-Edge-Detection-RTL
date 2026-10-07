import serial
from PIL import Image

PORT = "COM9"
BAUD = 115200

OUTPUT_RAW = "rx.raw"
OUTPUT_PNG = "rx_result.png"

ser = serial.Serial(PORT, BAUD, timeout=60)

while True:
    line = ser.readline().decode(errors="ignore").strip()
    if not line:
        continue

    print("HEADER:", line)

    if line.startswith("IMG "):
        parts = line.split()
        if len(parts) != 3:
            raise ValueError(f"잘못된 헤더 형식: {line}")

        width = int(parts[1])
        height = int(parts[2])
        break

nbytes = width * height
print(f"Receiving {nbytes} bytes...")

data = ser.read(nbytes)

if len(data) != nbytes:
    raise ValueError(f"수신 바이트 부족: expected={nbytes}, got={len(data)}")

with open(OUTPUT_RAW, "wb") as f:
    f.write(data)

img = Image.frombytes("L", (width, height), data)
img.save(OUTPUT_PNG)

print(f"Saved {OUTPUT_RAW}")
print(f"Saved {OUTPUT_PNG}")

tail = ser.readline().decode(errors="ignore").strip()
print("TAIL:", tail)

ser.close()
