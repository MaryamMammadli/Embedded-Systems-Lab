#include <Wire.h>
#include <uRTCLib.h>
//I2C (Inter-Integrated Circuit)
uRTCLib rtc(0x68);

const int PRESS = 10;
//                              D1 D2
const uint8_t digitPins[2]   = {4, 5};
//                              A  B  C  D  E   F   G   DP
const uint8_t segmentPins[8] = {6, 7, 8, 9, 10, 11, 12, A1};

const uint8_t buttonPin   = A0;
const uint8_t greenLedPin = A2;
const uint8_t redLedPin   = A3;

const uint8_t numbers[10][8] = {
  {1,1,1,1,1,1,0,0},
  {0,1,1,0,0,0,0,0},
  {1,1,0,1,1,0,1,0}, 
  {1,1,1,1,0,0,1,0},
  {0,1,1,0,0,1,1,0},
  {1,0,1,1,0,1,1,0},
  {1,0,1,1,1,1,1,0},
  {1,1,1,0,0,0,0,0},
  {1,1,1,1,1,1,1,0},
  {1,1,1,1,0,1,1,0}
};

int counter = 0;
uint8_t lastSecond = 0;

bool running = false;
bool started = false;

inline void writeSeg(uint8_t pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);
}

inline void enableDigit(uint8_t pin) {
  digitalWrite(pin, LOW);
}

inline void disableDigit(uint8_t pin) {
  digitalWrite(pin, HIGH);
}

bool buttonPressedEvent() {
  static bool lastStable;
  static bool lastRead;
  static unsigned long tChange = 0;
  const unsigned long DEBOUNCE_WAIT = 50;

  bool now = digitalRead(buttonPin);

  if (now != lastRead) {
    lastRead = now;
    tChange = millis();
  }

  if (millis() - tChange >= DEBOUNCE_WAIT) {
    if (lastStable != lastRead) {
      lastStable = lastRead;
      if (lastStable == LOW) return true; // pressed (INPUT_PULLUP)
    }
  }

  return false;
}

void setSegmentsForDigit(int value) {
  for (int i = 0; i < 8; i++) {
    writeSeg(segmentPins[i], numbers[value][i] == 1);
  }
}

void showDigitOnPosition(int value, int pos) {
  disableDigit(digitPins[0]);
  disableDigit(digitPins[1]);

  setSegmentsForDigit(value);
  enableDigit(digitPins[pos]);

  delay(4);
}

void displayTwoDigits(int num) {
  int tens = num / 10;
  int ones = num % 10;

  showDigitOnPosition(ones, 1);
  showDigitOnPosition(tens, 0);
}

void showResultLED() {
  if (counter == PRESS) {
    digitalWrite(greenLedPin, HIGH);
    digitalWrite(redLedPin, LOW);
  } else {
    digitalWrite(redLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
  }

  delay(800);

  digitalWrite(greenLedPin, LOW);
  digitalWrite(redLedPin, LOW);
}

void setup() {
  Wire.begin();
  pinMode(buttonPin, INPUT_PULLUP);

  pinMode(greenLedPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);

  for (int i = 0; i < 8; i++) pinMode(segmentPins[i], OUTPUT);
  for (int i = 0; i < 2; i++) pinMode(digitPins[i], OUTPUT);

  disableDigit(digitPins[0]);
  disableDigit(digitPins[1]);

  counter = 0;
}

void loop() {
  displayTwoDigits(counter);

  if (buttonPressedEvent()) {
    if (!running && !started) {
      // FIRST PRESS → START
      counter = 0;
      rtc.refresh();
      lastSecond = rtc.second();

      running = true;
      started = true;
    }
    else if (running) {
      // SECOND PRESS → STOP
      running = false; // if not it will not stop
      showResultLED();

      counter = 0;
      started = false;
    }
  }

  // COUNTING USING DS1307
  if (running) {
    rtc.refresh();
    uint8_t sec = rtc.second();

    if (sec != lastSecond) {
      lastSecond = sec;
      counter++;

      if (counter > 10) counter = 0;
    }
  }
}
