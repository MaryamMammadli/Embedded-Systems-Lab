#include "Servo.h"
#include <Stepper.h>

// Number of steps for one full rotation
const int stepsPerRevolution = 2048;  // 28BYJ-48

// Create Stepper object
Stepper myStepper(stepsPerRevolution, 11, 9, 10, 8);

// Create Servo object
Servo myservo;

// Pin connections
int servoPin = 12; 
int buzzer = 3;
int buttonPlayer1 = 6;
int buttonPlayer2 = 5;

// Game score variables
int score1 = 0;
int score2 = 0;

// Variables for reaction time
unsigned long startTime;
unsigned long reactionTime1;
unsigned long reactionTime2;

// Variable to track stepper position
int stepperPosition = 0;

void setup() {

  // Start Serial Monitor
  Serial.begin(9600);

  // Set stepper motor speed
  myStepper.setSpeed(10);

  // Attach servo motor to pin
  myservo.attach(servoPin);

  // Set button pins as input with pull-up resistors
  pinMode(buttonPlayer1, INPUT_PULLUP);
  pinMode(buttonPlayer2, INPUT_PULLUP);

  // Set buzzer pin as output
  pinMode(buzzer, OUTPUT);

  // Generate random values
  randomSeed(analogRead(A0));

  // Move servo to middle position
  myservo.write(90);

  // Print message
  Serial.println("GAME STARTS AUTOMATICALLY");
}

void loop() {

  // Reset game values
  resetGame();

  // Start game
  playGame();

  // Wait before restarting game
  delay(5000);
}

void playGame() {
  
  // Keep servo in middle position
  myservo.write(90);

  // Continue until one player gets 3 points
  while (score1 < 3 && score2 < 3) {

    Serial.println("NEW ROUND");

    // Random waiting time from 1 to 20 seconds
    int delayTime = random(1000, 20001);

    // Save current time
    unsigned long waitStart = millis();

    // WAIT PHASE
    // Check for false starts
    while (millis() - waitStart < delayTime) {

      // Player 1 pressed too early
      if (digitalRead(buttonPlayer1) == LOW) {

        Serial.println("FALSE START P1");

        // Give point to Player 2
        score2++;

        // Move stepper toward Player 2
        updateStepper(2);

        delay(1000);
        return;
      }

      // Player 2 pressed too early
      if (digitalRead(buttonPlayer2) == LOW) {

        Serial.println("FALSE START P2");

        // Give point to Player 1
        score1++;

        // Move stepper toward Player 1
        updateStepper(1);

        delay(1000);
        return;
      }
    }

    // BUZZER SIGNAL
    tone(buzzer, 1000);

    // Sound buzzer for 200 ms
    delay(200);

    // Stop buzzer
    noTone(buzzer);

    // Save reaction start time
    startTime = millis();

    // Variables to check button presses
    bool p1Pressed = false;
    bool p2Pressed = false;

    // REACTION PHASE
    while (!p1Pressed || !p2Pressed) {

      // Check Player 1 button
      if (!p1Pressed && digitalRead(buttonPlayer1) == LOW) {

        // Calculate reaction time
        reactionTime1 = millis() - startTime;

        p1Pressed = true;
      }

      // Check Player 2 button
      if (!p2Pressed && digitalRead(buttonPlayer2) == LOW) {

        // Calculate reaction time
        reactionTime2 = millis() - startTime;

        p2Pressed = true;
      }
    }

    // DETERMINE ROUND WINNER

    // Player 1 is faster
    if (reactionTime1 < reactionTime2) {

      // Increase Player 1 score
      score1++;

      Serial.print("P1 WIN: ");
      Serial.println(reactionTime1);

      // Move servo toward Player 1
      moveServo(1);

      // Move stepper toward Player 1
      updateStepper(1);
    } 
    
    // Player 2 is faster
    else {

      // Increase Player 2 score
      score2++;

      Serial.print("P2 WIN: ");
      Serial.println(reactionTime2);

      // Move servo toward Player 2
      moveServo(2);

      // Move stepper toward Player 2
      updateStepper(2);
    }

    // Small delay before next round
    delay(1500);
  }

  // FINAL WINNER

  // Player 1 wins game
  if (score1 == 3) {

    Serial.println("GAME WINNER: P1");

  } else {

    // Player 2 wins game
    Serial.println("GAME WINNER: P2");
  }

  // Rotate stepper motor one full turn
  victorySpin();
}

// Function to move servo
void moveServo(int player) {

  // Move left for Player 1
  if (player == 1) {

    myservo.write(0);

  } else {

    // Move right for Player 2
    myservo.write(180);
  }
}

// Function to update stepper position
void updateStepper(int player) {

  // Move forward for Player 1
  if (player == 1) {

    myStepper.step(50);

    stepperPosition += 50;

  } else {

    // Move backward for Player 2
    myStepper.step(-50);

    stepperPosition -= 50;
  }
}

// Function for winner celebration
void victorySpin() {

  // Rotate one full revolution
  myStepper.step(stepsPerRevolution);
}

// Function to reset game
void resetGame() {

  // Reset scores
  score1 = 0;
  score2 = 0;

  // Return stepper to starting position
  myStepper.step(-stepperPosition);

  // Reset position value
  stepperPosition = 0;

  // Move servo to middle
  myservo.write(90);
}
