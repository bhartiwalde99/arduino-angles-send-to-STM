#include <Arduino.h>

#define emgPin1 2  // Digital EMG sensor 1
#define emgPin2 3  // Digital EMG sensor 2

#define SHORT_FLEX_TIME 500
#define LONG_FLEX_TIME 1000
#define UART_SYNC_CODE 199

// ====== Grip pattern definitions ======
struct GripPattern {
  const char* name;
  uint8_t angles[6];
};

// 6 grip patterns (example for 6 fingers)
GripPattern gripPatterns[6] = {
  {"OpenPalm",   {0, 0, 0, 0, 0, 0}},
  {"PowerGrip",    {90, 90, 90, 90, 90, 60}},
  {"LateralGrip",  {45, 0, 0, 0, 70, 45}},
  {"PinchGrip",    {60, 0, 0, 0, 80, 90}},
  {"TripodGrip",   {70, 70, 0, 0, 85, 90}},
  {"PointGrip",    {0, 90, 90, 90, 0, 0}}
};

// ====== Global state ======
int currentGripIndex = 0;
bool gripLocked = false;
bool comboActive = false;
int lastCombo = 0;
unsigned long comboStartTime = 0;

// ====== Print helper ======
void printGripInfo(int index) {
  Serial.println();
  Serial.println("=== GRIP PATTERN ===");
  Serial.print("Name: ");
  Serial.println(gripPatterns[index].name);
  Serial.print("Status: ");
  Serial.println(gripLocked ? "LOCKED" : "UNLOCKED");
  Serial.println("====================");
}

// ====== Send angles to STM32 ======
void sendAnglesToSTM32(int gripIndex) {
  // Send SYNC byte
  Serial.write((uint8_t)UART_SYNC_CODE);

  // Send 6 finger angles
  for (int i = 0; i < 6; i++) {
    Serial.write(gripPatterns[gripIndex].angles[i]);
  }

  // Debug output (optional)
  Serial.print("Sent Grip: ");
  Serial.print(gripPatterns[gripIndex].name);
  Serial.print(" | Angles: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(gripPatterns[gripIndex].angles[i]);
    Serial.print(" ");
  }
  Serial.println();
}

// ====== Detect EMG gestures ======
void detectGestures() {
  unsigned long now = millis();

  int emg1 = digitalRead(emgPin1);
  int emg2 = digitalRead(emgPin2);

  int combo = 0;
  if (emg1 == 0 && emg2 == 1) combo = 1;      // 01 combo → select/execute grip
  else if (emg1 == 1 && emg2 == 0) combo = 2; // 10 combo → lock/unlock grip

  // Start of combo
  if (combo != 0 && !comboActive) {
    comboActive = true;
    comboStartTime = now;
    lastCombo = combo;
  }

  // Combo end or changed
  if (comboActive && (combo == 0 || combo != lastCombo)) {
    comboActive = false;
    unsigned long duration = now - comboStartTime;

    // ===== 01 combo → Grip selection or execution =====
    if (lastCombo == 1) {
      if (duration > LONG_FLEX_TIME && !gripLocked) {
        // Change to next grip pattern
        int prevGrip = currentGripIndex;
        currentGripIndex = (currentGripIndex + 1) % 6;

        // Send OpenPalm first before switching grip
        if (prevGrip != currentGripIndex) {
          Serial.println("*** GRIP CHANGED — OPEN FIRST ***");
          sendAnglesToSTM32(0);  // OpenPalm
          delay(800);
          sendAnglesToSTM32(0);
          delay(500);
        }

        Serial.println("*** NEXT GRIP SELECTED ***");
        printGripInfo(currentGripIndex);
        sendAnglesToSTM32(currentGripIndex);
      }
      else if (duration > SHORT_FLEX_TIME && !gripLocked) {
        // Execute current grip
        Serial.println("*** EXECUTE GRIP ***");
        sendAnglesToSTM32(currentGripIndex);
      }
    }

    // ===== 10 combo → Lock/Unlock current grip =====
    else if (lastCombo == 2) {
      if (duration > SHORT_FLEX_TIME) {
        gripLocked = !gripLocked;
        Serial.println(gripLocked ? "*** GRIP LOCKED ***" : "*** GRIP UNLOCKED ***");
        printGripInfo(currentGripIndex);
      }
    }
  }

  // Rapid switch between combos (01 ↔ 10)
  if (comboActive && combo != 0 && combo != lastCombo) {
    comboStartTime = now;
    lastCombo = combo;
  }
}

// ====== Arduino Setup ======
void setup() {
  Serial.begin(9600);
  pinMode(emgPin1, INPUT);
  pinMode(emgPin2, INPUT);

  Serial.println("=== EMG Grip Command Sender ===");
  Serial.println("01: Select/Execute Grip  | 10: Lock/Unlock");
  Serial.println("------------------------------------------");
  printGripInfo(currentGripIndex);
}

// ====== Arduino Loop ======
void loop() {
  detectGestures();
  delay(100);
}
