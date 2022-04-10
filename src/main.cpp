#include <Arduino.h>
#include <Encoder.h>
#include <Bounce2.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>


/* PINS MOTOR  */

#define PIN_DRIVER_ENA 22          // ENA+ Pin
#define PIN_DRIVER_PUL 24          // PUL+ Pin
#define PIN_DRIVER_DIR 26          // DIR+ Pin
int motorSpeed = 500;
boolean motorDirection = LOW;

/* PINS ROTARY ENCODER */

#define PIN_ROTARY_ENCODER_CLK 27  // CLK
#define PIN_ROTARY_ENCODER_DT 25   // DT
#define PIN_ROTARY_ENCODER_SW 23   // Switch
long altePosition = -999;         // Definition der "alten" Position (Diese fiktive alte Position wird benötigt, damit die aktuelle Position später im seriellen Monitor nur dann angezeigt wird, wenn wir den Rotary Head bewegen)

// erzeuge ein neues Encoder Objekt
Encoder meinEncoder(PIN_ROTARY_ENCODER_DT, PIN_ROTARY_ENCODER_CLK);

/* PINS SWITCHES */

/*
SWITCH 1: 28    SWITCH 7: 40
SWITCH 2: 30    SWITCH 8: 42
SWITCH 3: 32    SWITCH 9: 44
SWITCH 4: 34    SWITCH 10: 46
SWITCH 5: 36    SWITCH 11: 48
SWITCH 6: 38    SWITCH 12: 50
SWITCH 13: 52
*/
#define NUM_BUTTONS 14
//const uint8_t BUTTON_PINS[NUM_BUTTONS] = {28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, PIN_ROTARY_ENCODER_SW};
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {44, 46, 48, 50, 36, 38, 40, 42, 28, 30, 32, 34, 52, PIN_ROTARY_ENCODER_SW};
Bounce * buttons = new Bounce[NUM_BUTTONS];
int buttonPressed = -1;

Bounce endStopA = Bounce();
Bounce endStopB = Bounce();

/* PINS LCD DISPLAY */
// pin 3 - Serial clock out (SCLK)
// pin 4 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 6 - LCD chip select (CS)
// pin 7 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(3, 4, 5, 6, 7);

int totalTrackSteps = 0;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS //////////////////////////////////////////////////////////////////////////////////

void DisplayClear();
void DisplayMessage(int x, int y, String message);
void MotorChangeDirection();
void MotorCalibrateEndStops();
void MotorStep();
void CheckButtons();
bool CheckEndStopA();
bool CheckEndStopB();
void UpdateDisplay();


/**********************************************************************************************************/
/* SETUP **************************************************************************************************/
void setup()
{
  Serial.begin(9600);

  /* LCD DISPLAY SETUP */
  display.begin();
  display.setContrast(57);
  delay(1000);
  display.clearDisplay();

  /* BUTTONS SETUP */

  // setup 14 button bounce objects
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i].attach( BUTTON_PINS[i] , INPUT_PULLUP  );        //setup the bounce instance for the current button
    buttons[i].interval(25);                                    // interval in ms
  }

  endStopA.attach( 8, INPUT_PULLUP );
  endStopA.interval(25);
  endStopB.attach( 9, INPUT_PULLUP );
  endStopB.interval(25);

  /* MOTOR SETUP */

  pinMode(PIN_DRIVER_DIR, OUTPUT);
  pinMode(PIN_DRIVER_PUL, OUTPUT);

  // enable motor
  digitalWrite(PIN_DRIVER_ENA,LOW);

  // start calibration
  MotorCalibrateEndStops();
}





/*********************************************************************************************************/
/* LOOP **************************************************************************************************/
void loop()
{
  /* BUTTONS CHECK LOOP */
  CheckButtons();
  CheckEndStopA();
  CheckEndStopB();

  /* MOTOR LOOP */
/*
  long neuePosition = meinEncoder.read(); // Die "neue" Position des Encoders wird definiert. Dabei wird die aktuelle Position des Encoders über die Variable.Befehl() ausgelesen.
  if (neuePosition != altePosition) // Sollte die neue Position ungleich der alten (-999) sein (und nur dann!!)...
  {
    altePosition = neuePosition;
    motorSpeed = 500 - neuePosition;
    if ( motorSpeed < 50 ) {
      motorSpeed = 50;
    }
    else if ( motorSpeed > 2000 )
    {
      motorSpeed = 2000;
    }
    Serial.println(pd); // ...soll die aktuelle Position im seriellen Monitor ausgegeben werden.
  }

  digitalWrite(PIN_DRIVER_DIR, motorDirection);
  digitalWrite(PIN_DRIVER_PUL, HIGH);
  delayMicroseconds(pd);
  digitalWrite(PIN_DRIVER_PUL, LOW);
  delayMicroseconds(pd);
*/
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS ///////////////////////////////////////////////////////////////////////////////////

/*
*  CheckButtons()
*/
void CheckButtons()
{
  for (int i = 0; i < NUM_BUTTONS; i++)  {
    // Update the Bounce instance :
    buttons[i].update();
    // If button has been pressed
    if ( buttons[i].fell() ) {
      //TODO
      Serial.print("Button pressed: ");
      Serial.println(i);
  
      buttonPressed = i;

      // check rotary encoder button press
      if ( i == 13 )
      {
        Serial.println("Motor Clibaration");
        MotorChangeDirection();
      }

      UpdateDisplay();
    }
  }
}


/*****************************************************
*  CheckEndStopA()
*/
bool CheckEndStopA()
{
  endStopA.update();

  if ( endStopA.fell() ) 
  {
    Serial.println("endStopA betaetigt");
    return true;
  }

  return false;
}


/*****************************************************
*  CheckEndStopB()
*/
bool CheckEndStopB()
{
  endStopB.update();

  if ( endStopB.fell() ) 
  {
    Serial.println("endStopB betaetigt");
    return true;
  }

  return false;
}


/*****************************************************
* MotorChangeDirection()
* Changes the direction of the motor
*/
void MotorChangeDirection()
{
  motorDirection = !motorDirection;
}


/*****************************************************
*  MotorCalibrateEndStops()
*/
void MotorCalibrateEndStops()
{
  Serial.println("MotorCalibrateEndStops()");

  DisplayClear();
  DisplayMessage(0, 0, "Kalibrierung ...");

  bool hasFirstEndStopTriggered = false;
  bool hasSecondEndStopTriggered = false;

  // Goto first End Stop
  while( hasFirstEndStopTriggered == false )
  {
    MotorStep();
    if ( CheckEndStopA() == true || CheckEndStopB() == true )
    {
      MotorChangeDirection();
      hasFirstEndStopTriggered = true;
      DisplayMessage(0, 10, "Endstop 1");
    }
  }

  // Goto second End Stop
  while( hasSecondEndStopTriggered == false )
  {
    MotorStep();
    totalTrackSteps++;

    if ( CheckEndStopA() == true || CheckEndStopB() == true )
    {
      MotorChangeDirection();
      hasSecondEndStopTriggered = true;
      DisplayMessage(0, 20, "Endstop 2");
      DisplayMessage(0, 30, String(totalTrackSteps));
    }
  }

  // If endstop A triggers
  // Goto endstop B and record each step until enstop B is triggered

  Serial.println("Calibration finished");
  Serial.print("Total track steps:");
  Serial.println(totalTrackSteps);
}


/*****************************************************
*  MotorStep()
* do a single motor step
*/
void MotorStep()
{
  digitalWrite(PIN_DRIVER_DIR, motorDirection);
  digitalWrite(PIN_DRIVER_PUL, HIGH);
  delayMicroseconds(motorSpeed);
  digitalWrite(PIN_DRIVER_PUL, LOW);
  delayMicroseconds(motorSpeed);
}


/*****************************************************
*  UpdateDisplay()
*/
void UpdateDisplay()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);

  display.setCursor(0,0);
  display.println("Speed: ");
  display.setCursor(45,0);
  display.println(motorSpeed);

  display.setCursor(0,10);
  display.println("Button: ");
  display.setCursor(45,10);
  display.println(buttonPressed);

  display.display();
}


/*****************************************************
*  DisplayClear()
*/
void DisplayClear()
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(BLACK);
}


/*****************************************************
*  DisplayMessage(int x, int y, String message)
*/
void DisplayMessage(int x, int y, String message)
{
  display.setCursor(x,y);
  display.println(message);
  display.display();
}