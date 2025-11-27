import serial
import csv
import time
from datetime import datetime

PORT = 'COM3'        # 아두이노 포트 번호
BAUD = 115200

# --- 자동 파일명 생성 ---
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
OUTPUT = f"ppg_{timestamp}.csv"

# 아두이노 기록 시간(초)
# 아두이노는 3초 ARM + 60초 RECORDING 이라 여유 있게 65초로 설정
DURATION = 65  

arduino = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # 연결 안정화 (아두이노 리셋 후 부팅 시간)

data = []
header_saved = False  # LABEL(헤더)는 한 번만 저장하기 위한 플래그

print("Waiting for Arduino to be ready...")

# --- 아두이노에서 READY 메시지 올 때까지 대기하면서
#     LABEL 줄이 오면 미리 헤더로 저장 ---
while True:
    line = arduino.readline().decode(errors='ignore').strip()
    if not line:
        continue

    # 디버그용 출력 보고 싶으면 주석 해제
    # print(">>", line)

    # CLEARDATA는 무시
    if line.startswith("CLEARDATA"):
        continue

    # LABEL 줄이면 헤더로 저장 (한 번만)
    if line.startswith("LABEL,") and not header_saved:
        header = line.split(',')[1:]   # "LABEL" 빼고 나머지 컬럼
        data.append(header)
        header_saved = True
        print("Header received:", header)
        continue

    # Ready 메시지 오면 루프 종료
    if "Ready." in line:
        print("Arduino is ready.")
        break

# 아두이노에게 's' 보내서 측정 시작시키기
arduino.write(b's\n')
print(f"Sent 's' to Arduino. Logging for ~{DURATION} seconds...")

start_time = time.time()

try:
    while time.time() - start_time < DURATION:
        line = arduino.readline().decode(errors='ignore').strip()
        if not line:
            continue

        # LABEL이 혹시 다시 와도, 이미 저장했다면 무시
        if line.startswith("LABEL,"):
            if not header_saved:
                header = line.split(',')[1:]
                data.append(header)
                header_saved = True
                print("Header received (in main loop):", header)
            continue

        # DATA 줄이면 실제 값
        if line.startswith("DATA,"):
            row = line.split(',')[1:]      # "DATA" 빼고 나머지 값
            data.append(row)
            continue

        # 나머지(CLEARDATA, Ready..., # START 등)는 무시
        # 필요하면 디버깅용 출력 가능
        # print("OTHER:", line)

except KeyboardInterrupt:
    print("Stopped early by user (Ctrl+C).")

print("Time up. Saving CSV...")

with open(OUTPUT, 'w', newline='', encoding='utf-8') as f:
    writer = csv.writer(f)
    writer.writerows(data)

arduino.close()

print(f"Saved to {OUTPUT}")
