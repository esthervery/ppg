#include <Arduino_LSM9DS1.h>

const int PPG_PIN = A0;
const uint32_t TARGET_INTERVAL_US = 10000; // 100 Hz (10ms)
const uint32_t DURATION_MS        = 60000; // 60s
const uint32_t ARM_MS             = 3000;  // 3s 안정화

enum State { IDLE, ARMING, RECORDING, DONE };
State state = IDLE;

uint32_t last_t = 0;
uint32_t start_t_us = 0;
uint32_t start_t_ms = 0;
uint32_t next_due = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!IMU.begin()) {
    Serial.println("IMU init failed!");
    while (1);
  }

  // -------------------------------
  //   PLX-DAQ Header (condition 제거)
  // -------------------------------
  Serial.println("CLEARDATA");
  Serial.println("LABEL,t_us,dt_us,ax,ay,az,ppg");

  Serial.println("Ready. Type 's' to start, 'q' to stop.");
}

void loop() {

  // --- Serial command handling ---
  if (Serial.available()) {
    char c = Serial.read();

    if (c == 's' && state == IDLE) {
      Serial.println("Arming...");
      state = ARMING;
      start_t_ms = millis();
    }

    if (c == 'q' && state == RECORDING) {
      Serial.println("Stopped.");
      state = DONE;
    }
  }

  // --------------------------
  //     ARMING → RECORDING
  // --------------------------
  if (state == ARMING) {

    if (millis() - start_t_ms >= ARM_MS) {
      state = RECORDING;

      start_t_us = micros();
      last_t = start_t_us;
      next_due = start_t_us + TARGET_INTERVAL_US;

      Serial.println("# START");
    }
    return;
  }

  // --------------------------
  //     RECORDING (100Hz)
  // --------------------------
  if (state == RECORDING) {

    // 자동 종료
    if (millis() - (start_t_ms + ARM_MS) >= DURATION_MS) {
      Serial.println("# END auto");
      state = DONE;
      return;
    }

    uint32_t now = micros();
    if ((int32_t)(now - next_due) < 0) {
      return; // 아직 샘플 타임 아님
    }
    next_due += TARGET_INTERVAL_US; // drift-free scheduling

    // --- Read PPG ---
    int ppg = analogRead(PPG_PIN);

    // --- Read Accelerometer only ---
    float ax = 0, ay = 0, az = 0;

    if (IMU.accelerationAvailable()) {
      IMU.readAcceleration(ax, ay, az);
    }

    // --- Timestamp ---
    uint32_t t_us  = now;
    uint32_t dt_us = t_us - last_t;
    last_t = t_us;

    // --------- PLX-DAQ DATA 출력 ---------
    Serial.print("DATA,");
    Serial.print(t_us);  Serial.print(',');
    Serial.print(dt_us); Serial.print(',');
    Serial.print(ax, 6); Serial.print(',');
    Serial.print(ay, 6); Serial.print(',');
    Serial.print(az, 6); Serial.print(',');
    Serial.println(ppg);

    return;
  }

  // --------------------------
  //         DONE
  // --------------------------
  if (state == DONE) {
    Serial.println("END. Press 's' to start again.");
    state = IDLE;
  }
}

