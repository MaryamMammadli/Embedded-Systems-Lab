#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <IRremote.hpp>

#define RFID_SS_PIN   10
#define RFID_RST_PIN  9

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

#define IR_PIN A1

#define GREEN_LED A3
#define RED_LED   A2

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Keypad pins
byte rowPins[ROWS] = {8, 7, 6, 5};   // R1, R2, R3, R4
byte colPins[COLS] = {4, 3, 2, A0};     // C1, C2, C3, C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// makeKeymap() function of the Keypad library using they previously defined keys array
// rowPins is the array holding the pin numbers of the row pins
// colPins is the array holding the pin numbers of the column pins
// ROWS is the number of rows in the keypad
// COLS is the number of columns in the keypad

enum SystemState {
  WAITING_CODE,
  LOCKED,
  UNLOCKED
};

SystemState state = WAITING_CODE;

// code storage
String lockCode = "";       // final saved password
String keypadBuffer = "";   // temporary input from keypad before pressing #
String irBuffer = "";       // temporary input from IR remote used to unlock

// led timer
unsigned long lastBlinkTime = 0;
bool blinkState = false;

// set system state
void setState(SystemState newState) {
  state = newState;

  if (state == WAITING_CODE) {
    rfid.PCD_AntennaOff(); // turns off RFID antenna
    Serial.println("STATE, WAITING_CODE");
  }

  else if (state == LOCKED) {
    rfid.PCD_AntennaOff(); // turns off RFID antenna
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    Serial.println("STATE, LOCKED");
  }

  else if (state == UNLOCKED) {
    rfid.PCD_AntennaOn(); // turns on RFID antenna
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    Serial.println("STATE, UNLOCKED");
  }
}

// update led patterns
void updateLEDs() {
  if (state == WAITING_CODE) {
    if (millis() - lastBlinkTime >= 500) {
      lastBlinkTime = millis();
      blinkState = !blinkState;

      digitalWrite(GREEN_LED, blinkState);
      digitalWrite(RED_LED, !blinkState);
    }
  }

  else if (state == LOCKED) {
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }

  else if (state == UNLOCKED) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }
}

// RFID success flash
void flashRFIDRead() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, HIGH);
    delay(120);

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, LOW);
    delay(120);
  }

  updateLEDs();
}

// error flash
void flashError() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED, HIGH);
    delay(150);
    digitalWrite(RED_LED, LOW);
    delay(150);
  }

  updateLEDs();
}

// keypad handle
void handleKeypad() {
  char key = keypad.getKey();

  if (!key) return;

  // keypad is used only when system is NOT locked
  if (state == LOCKED) return;

  // // NEED TO BE REMOVED!!!!!!!!!!!
  // if (key) {
  //   Serial.print("RAW_KEY,");
  //   Serial.println(key);
  }
  //

  if (key >= '0' && key <= '9') {
    if (keypadBuffer.length() < 4) {
      keypadBuffer += key;
      Serial.print("KEYPAD_DIGIT,");
      Serial.println(key);
    }
  }

  // for clearing
  else if (key == '*') {
    keypadBuffer = "";
    Serial.println("KEYPAD_CLEAR");
  }

  // for confirming
  else if (key == '#') {
    if (keypadBuffer.length() == 4) {
      lockCode = keypadBuffer;
      keypadBuffer = "";

      Serial.print("LOCK_CODE_SET,");
      Serial.println(lockCode);

      setState(LOCKED);
    } 
    else {
      Serial.println("ERROR,ENTER_4_DIGITS_FIRST");
      flashError();
      keypadBuffer = "";
    }
  }
}

// map IR remote buttons sends hexadecimal command
// these command values are common for Keyes IR remote
// if your remote is different, open Serial Monitor and check IR_COMMAND output
char irCommandToDigit(uint8_t command) {
  switch (command) {
    case 0x16: return '0';
    case 0x0C: return '1';
    case 0x18: return '2';
    case 0x5E: return '3';
    case 0x08: return '4';
    case 0x1C: return '5';
    case 0x5A: return '6';
    case 0x42: return '7';
    case 0x52: return '8';
    case 0x4A: return '9';
    default: return '\0';
  }
}

// IR handling 
void handleIR() {
  if (IrReceiver.decode()) {
    uint8_t command = IrReceiver.decodedIRData.command;

    Serial.print("IR_COMMAND,0x");
    Serial.println(command, HEX);

    // Ignore repeat signal when button is held
    if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {

      if (state == LOCKED) {
        char digit = irCommandToDigit(command); // converts IR command into digit

        if (digit != '\0') {
          irBuffer += digit;

          Serial.print("IR_DIGIT,");
          Serial.println(digit);

          if (irBuffer.length() == 4) {
            if (irBuffer == lockCode) {
              Serial.println("UNLOCK_SUCCESS");
              irBuffer = "";
              setState(UNLOCKED);
            } 
            else {
              Serial.println("UNLOCK_FAILED");
              irBuffer = "";
              flashError();
            }
          }
        }
      }
    }

    IrReceiver.resume();
  }
}

// RFID UID to string
String getUIDString() {
  String uidString = "";

  for (byte i = 0; i < rfid.uid.size; i++) { // 4-7 bytes usually
    if (rfid.uid.uidByte[i] < 0x10) {
      uidString += "0";
    }

    uidString += String(rfid.uid.uidByte[i], HEX); // adds the current UID byte in hexadecimal format

    if (i < rfid.uid.size - 1) {
      uidString += ":";
    }
  }

  uidString.toUpperCase();
  return uidString;
}

// RFID handling
void handleRFID() {
  // RFID works only in unlocked state
  if (state != UNLOCKED) return;

  if (!rfid.PICC_IsNewCardPresent()) return;  // if no card is present, stop
  if (!rfid.PICC_ReadCardSerial()) return;    // if reading fails, stop

  String uid = getUIDString(); // converts UID to text format

  Serial.print("TAG,");
  Serial.println(uid);
  // send to serial monitor + GUI
  flashRFIDRead();

  rfid.PICC_HaltA();      // stops communication with the current RFID card
  rfid.PCD_StopCrypto1(); // stops encryption/communication session with the card
}

void setup() {
  Serial.begin(9600);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK); // start IR receiver on IR_PIN and enable feedback LED when signal is received

  setState(WAITING_CODE); // start system in waiting mode, ready to receive keypad code

  Serial.println("SYSTEM_READY");
  Serial.println("Use keypad: enter 4 digits, then press # to lock.");
  Serial.println("Use IR remote: enter same 4 digits to unlock.");
  Serial.println("RFID works only when system is unlocked.");
}

void loop() {
  updateLEDs();

  handleKeypad();
  handleIR();
  handleRFID();
}
