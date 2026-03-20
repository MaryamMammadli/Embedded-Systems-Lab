#include <LiquidCrystal.h>          // Library to control 16x2 LCD (HD44780)

// LiquidCrystal(rs, en, d4, d5, d6, d7)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const int soundPin = A0;           
const int ledPin = 8;

const int threshold = 600;          
unsigned long previousMillis = 0;
const long interval = 100;

void setup()
{
  Serial.begin(9600);               

  pinMode(ledPin, OUTPUT);          

  lcd.begin(16,2);  // LCD 16 columns and 2 rows
  lcd.clear();                     
  lcd.setCursor(0,0);  //row 0, column 0
  lcd.print("Sound Monitor");      
}

void loop()
{
  unsigned long currentMillis = millis();   
  
  if(currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;   
    int soundValue = analogRead(soundPin); 
    
    lcd.setCursor(0,1);                     
    lcd.print("Lvl:");                      
    lcd.print(soundValue);                  
    lcd.print("    ");
    
    Serial.print("SOUND:");
    Serial.println(soundValue);
 
    if(soundValue > threshold) {
      digitalWrite(ledPin, HIGH);  

      lcd.setCursor(10,1);                
      lcd.print("ALERT");         
    } else {
      digitalWrite(ledPin, LOW);        

      lcd.setCursor(10,1);              
      lcd.print("     ");    
    }
  }
}
