#include <Arduino.h>

const int LRpin = A0;
const int UDpin = A1;

int LR_neutral = 0;
int UD_neutral = 0;

const int deadzone = 10;

// LED pins (PWM where possible)
const int Rpin = 11;  // DOWN -> Red
const int Ypin = 10;  // LEFT -> Yellow
const int Gpin = 6;   // RIGHT -> Green
const int Bpin = 9;   // UP -> Blue

// send rate for Python GUI
const uint16_t period_ms = 20; // 50 Hz
unsigned long lastSend = 0;

static void calibrateNeutral() {
  long sumLR = 0, sumUD = 0;
  const int N = 50;

  delay(200); // ADC settle

  for (int i = 0; i < N; i++) {
    sumLR += analogRead(LRpin);
    sumUD += analogRead(UDpin);
    delay(5);
  }
  LR_neutral = (int)(sumLR / N);
  UD_neutral = (int)(sumUD / N);
}

static const char* getDir(int dx, int dy) {
  if (abs(dx) <= deadzone && abs(dy) <= deadzone) return "CENTER";
  if (abs(dx) > abs(dy)) return (dx > 0) ? "RIGHT" : "LEFT";
  return (dy > 0) ? "DOWN" : "UP";
}

void setup() {
  Serial.begin(9600);

  pinMode(Rpin, OUTPUT);
  pinMode(Ypin, OUTPUT);
  pinMode(Gpin, OUTPUT);
  pinMode(Bpin, OUTPUT);

  calibrateNeutral();

  // Optional: one boot line (Python ignores non-CSV safely)
  Serial.println("BOOT");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSend < period_ms) return;
  lastSend = now;

  int LR = analogRead(LRpin);
  int UD = analogRead(UDpin);

  int dx = LR - LR_neutral;
  int dy = UD - UD_neutral;

  const char* dir = getDir(dx, dy);

  // --- LED brightness control ---
  // Reset brightness
  int R = 0, Y = 0, G = 0, B = 0;

  // UP/DOWN via UD axis
  if (UD >= UD_neutral + deadzone) {
    // DOWN -> Red brightness increases as UD increases
    R = map(UD, UD_neutral + deadzone, 1023, 0, 255);
  } else if (UD <= UD_neutral - deadzone) {
    // UP -> Blue brightness increases as UD decreases
    B = map(UD, UD_neutral - deadzone, 0, 0, 255);
  }

  // RIGHT/LEFT via LR axis
  if (LR >= LR_neutral + deadzone) {
    // RIGHT -> Green brightness increases as LR increases
    G = map(LR, LR_neutral + deadzone, 1023, 0, 255);
  } else if (LR <= LR_neutral - deadzone) {
    // LEFT -> Yellow brightness increases as LR decreases
    Y = map(LR, LR_neutral - deadzone, 0, 0, 255);
  }

  // Safety clamp
  R = constrain(R, 0, 255);
  Y = constrain(Y, 0, 255);
  G = constrain(G, 0, 255);
  B = constrain(B, 0, 255);

  analogWrite(Rpin, R);
  analogWrite(Ypin, Y);
  analogWrite(Gpin, G);
  analogWrite(Bpin, B);

  // --- IMPORTANT: CSV ONLY for GUI ---
  // Format: LR,UD,dx,dy,DIR
  Serial.print(LR); Serial.print(",");
  Serial.print(UD); Serial.print(",");
  Serial.print(dx); Serial.print(",");
  Serial.print(dy); Serial.print(",");
  Serial.println(dir);
}
