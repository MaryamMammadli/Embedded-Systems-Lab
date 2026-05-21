#include <SPI.h>          // SPI communication library
#include <MFRC522.h>      // RFID library
#include <Keypad.h>       // Keypad library
#include <IRremote.hpp>   // IR remote library

// RFID module pins
#define RFID_SS_PIN   10
#define RFID_RST_PIN  9

// Create RFID object
MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

// IR receiver pin
#define IR_PIN A1

// LED pins
#define GREEN_LED A3
#define RED_LED   A2

// Keypad size
const byte ROWS = 4;
const byte COLS = 4;

// Keypad button layout
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Keypad row pins
byte rowPins[ROWS] = {8, 7, 6, 5};

// Keypad column pins
byte colPins[COLS] = {4, 3, 2, A0};

// Create keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// makeKeymap() creates keypad layout
// rowPins stores row pin numbers
// colPins stores column pin numbers
// ROWS is total rows
// COLS is total columns

// System states
enum SystemState {
  WAITING_CODE,
  LOCKED,
  UNLOCKED
};

// Current system state
SystemState state = WAITING_CODE;

// Password variables
String lockCode = "";       // saved password
String keypadBuffer = "";   // keypad input buffer
String irBuffer = "";       // IR remote input buffer

// LED blinking variables
unsigned long lastBlinkTime = 0;
bool blinkState = false;

// Function to change system state
void setState(SystemState newState) {

  // Save new state
  state = newState;

  // WAITING_CODE state
  if (state == WAITING_CODE) {

    // Turn RFID antenna off
    rfid.PCD_AntennaOff();

    Serial.println("STATE, WAITING_CODE");
  }

  // LOCKED state
  else if (state == LOCKED) {

    // Turn RFID antenna off
    rfid.PCD_AntennaOff();

    // Green LED OFF
    digitalWrite(GREEN_LED, LOW);

    // Red LED ON
    digitalWrite(RED_LED, HIGH);

    Serial.println("STATE, LOCKED");
  }

  // UNLOCKED state
  else if (state == UNLOCKED) {

    // Turn RFID antenna ON
    rfid.PCD_AntennaOn();

    // Green LED ON
    digitalWrite(GREEN_LED, HIGH);

    // Red LED OFF
    digitalWrite(RED_LED, LOW);

    Serial.println("STATE, UNLOCKED");
  }
}

// Function to update LED behavior
void updateLEDs() {

  // Waiting mode LED blinking
  if (state == WAITING_CODE) {

    // Blink every 500 ms
    if (millis() - lastBlinkTime >= 500) {

      // Save current time
      lastBlinkTime = millis();

      // Change blink state
      blinkState = !blinkState;

      // Alternate LEDs
      digitalWrite(GREEN_LED, blinkState);
      digitalWrite(RED_LED, !blinkState);
    }
  }

  // Locked mode LEDs
  else if (state == LOCKED) {

    // Green OFF
    digitalWrite(GREEN_LED, LOW);

    // Red ON
    digitalWrite(RED_LED, HIGH);
  }

  // Unlocked mode LEDs
  else if (state == UNLOCKED) {

    // Green ON
    digitalWrite(GREEN_LED, HIGH);

    // Red OFF
    digitalWrite(RED_LED, LOW);
  }
}

// Function for RFID success flash
void flashRFIDRead() {

  // Repeat 3 times
  for (int i = 0; i < 3; i++) {

    // Turn both LEDs ON
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, HIGH);

    delay(120);

    // Turn both LEDs OFF
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);

    delay(120);
  }

  // Restore normal LED state
  updateLEDs();
}

// Function for error flashing
void flashError() {

  // Repeat 3 times
  for (int i = 0; i < 3; i++) {

    // Turn red LED ON
    digitalWrite(RED_LED, HIGH);

    delay(150);

    // Turn red LED OFF
    digitalWrite(RED_LED, LOW);

    delay(150);
  }

  // Restore normal LED state
  updateLEDs();
}

// Function to handle keypad input
void handleKeypad() {

  // Read keypad button
  char key = keypad.getKey();

  // Stop if no key pressed
  if (!key) return;

  // Ignore keypad if system is locked
  if (state == LOCKED) return;

  // If pressed key is a number
  if (key >= '0' && key <= '9') {

    // Limit password length to 4 digits
    if (keypadBuffer.length() < 4) {

      // Add digit to buffer
      keypadBuffer += key;

      Serial.print("KEYPAD_DIGIT,");
      Serial.println(key);
    }
  }

  // Clear input with *
  else if (key == '*') {

    // Clear keypad buffer
    keypadBuffer = "";

    Serial.println("KEYPAD_CLEAR");
  }

  // Confirm password with #
  else if (key == '#') {

    // Check if exactly 4 digits entered
    if (keypadBuffer.length() == 4) {

      // Save password
      lockCode = keypadBuffer;

      // Clear buffer
      keypadBuffer = "";

      Serial.print("LOCK_CODE_SET,");
      Serial.println(lockCode);

      // Change state to LOCKED
      setState(LOCKED);
    } 
    
    else {

      // Show error message
      Serial.println("ERROR,ENTER_4_DIGITS_FIRST");

      // Flash error LEDs
      flashError();

      // Clear buffer
      keypadBuffer = "";
    }
  }
}
