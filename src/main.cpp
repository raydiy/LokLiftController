#include <Arduino.h>
#include <Encoder.h>
#include <Bounce2.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROM.h>


/********** PINS MOTOR ***************************************************/
#define PIN_DRIVER_ENA 22          // ENA+ Pin
#define PIN_DRIVER_PUL 24          // PUL+ Pin
#define PIN_DRIVER_DIR 26          // DIR+ Pin
/*************************************************************************/


/********** PINS ROTARY ENCODER ******************************************/
#define PIN_ROTARY_ENCODER_CLK 27  // CLK
#define PIN_ROTARY_ENCODER_DT 25   // DT
#define PIN_ROTARY_ENCODER_SW 23   // Switch
// erzeuge ein neues Encoder Objekt
Encoder rotaryEncoder(PIN_ROTARY_ENCODER_DT, PIN_ROTARY_ENCODER_CLK);
/*************************************************************************/


/********** PINS SWITCHES ************************************************/
/*
SWITCH 1: 44    SWITCH 7: 40
SWITCH 2: 46    SWITCH 8: 42
SWITCH 3: 48    SWITCH 9: 28
SWITCH 4: 50    SWITCH 10: 30
SWITCH 5: 36    SWITCH 11: 32
SWITCH 6: 38    SWITCH 12: 34
SWITCH 13: 52
SWITCH 14: 23 (PIN_ROTARY_ENCODER_SW)
*/
#define NUM_BUTTONS 14
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {44, 46, 48, 50, 36, 38, 40, 42, 28, 30, 32, 34, 52, PIN_ROTARY_ENCODER_SW};
Bounce * buttons = new Bounce[NUM_BUTTONS];
/*************************************************************************/


/********** PINS END STOPS ***********************************************/
Bounce endStopA = Bounce();
Bounce endStopB = Bounce();
/*************************************************************************/


/********** PINS DISPLAY *************************************************/
// pin 3 - Serial clock out (SCLK)
// pin 4 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 6 - LCD chip select (CS)
// pin 7 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(3, 4, 5, 6, 7);
/*************************************************************************/


/********** GLOBALS ******************************************************/
int motorMinSpeed = 2000;         // base speed of the motor
int motorMaxSpeed = 50;
int motorSpeed = 0;               // speed changed by rotary encoder
boolean motorDirection = LOW;     // clockwise rotation
//long altePosition = -999;         // Definition der "alten" Position (Diese fiktive alte Position wird benötigt, damit die aktuelle Position später im seriellen Monitor nur dann angezeigt wird, wenn wir den Rotary Head bewegen)
int buttonPressed = -1;           // the array ID of the button that has been pressed last
unsigned int totalTrackSteps = 0;          // the number of steps from one end stop to the the other end stop
unsigned long startTime = 0;
unsigned long currentStepPosition = 0;
long encoderPosition = 0;
long oldEncoderPosition = 0;
byte motorMode = 0;             // different motorModes: 1 continuos, 2 single step
/*************************************************************************/



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS //////////////////////////////////////////////////////////////////////////////////
//
//
void DisplayClear();
void DisplayMessage(int x, int y, String message);
void MotorChangeDirection();
void MotorCalibrateEndStops();
void MotorStep();
void MotorModeSwitch();
bool CheckButton(byte id);
int CheckButtons();
bool CheckEndStopA();
bool CheckEndStopB();
void UpdateDisplay();



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP //////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void setup()
{
  Serial.begin(9600);

  /* LCD DISPLAY SETUP */
  display.begin();
  display.setContrast(57);
  delay(1000);
  display.clearDisplay();
  
  DisplayMessage(0, 0, "Starte LokLift Controller...");

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

  // check five seconds for button presses during statup to enter configuration modes
  startTime = millis();
  while( millis() < startTime + 5000 )
  {
    CheckButtons();
  }

  // if button 12 is pressed on startup then start calibration
  if ( buttonPressed == 12 )
  {
    MotorCalibrateEndStops();
  }

  DisplayClear();
  DisplayMessage(0, 0, "Bahn frei!");
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP ///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void loop()
{
  /* BUTTONS CHECK LOOP */
  CheckButtons();
  CheckEndStopA();
  CheckEndStopB();


  encoderPosition = rotaryEncoder.read();
  if ( oldEncoderPosition != encoderPosition )
  {
    oldEncoderPosition = encoderPosition;
    Serial.println(encoderPosition);
  }
  
  // check for rotay encoder knob switch press
  if ( buttonPressed == 13 ) 
  { 
    MotorModeSwitch();
    oldEncoderPosition = 0;
    encoderPosition = 0;
    rotaryEncoder.write(0);
    buttonPressed = -1;
  }

  // continuous motor mode
  // rotary encoder controlls the speed
  if ( motorMode == 0 ) 
  {
    if( encoderPosition != 0 )
    {
      motorDirection = encoderPosition > 0 ? HIGH : LOW;
      motorSpeed = motorMinSpeed - abs(encoderPosition);
      MotorStep();
    }
  }

  // step motor mode
  // each encoder step is a single motor step
  else if ( motorMode == 1 )
  {
    // Single Motor Step Mode
    if( encoderPosition != 0 )
    {
      motorDirection = encoderPosition > 0 ? HIGH : LOW;
      MotorStep();
      oldEncoderPosition = 0;
      encoderPosition = 0;
      rotaryEncoder.write(0);
    }
  }



  /* MOTOR LOOP */
/*
  long neuePosition = rotaryEncoder.read(); // Die "neue" Position des Encoders wird definiert. Dabei wird die aktuelle Position des Encoders über die Variable.Befehl() ausgelesen.
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
int CheckButtons()
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
      return i;
    }
  }

  return -1;
}


/*****************************************************
*  CheckButton(byte id)
* returns true if the button with the id is pressed
*/
bool CheckButton(byte id)
{
  Serial.print("CheckButton() ");
  Serial.println(id);
  
  buttons[id].update();
  
  if ( buttons[id].fell() ) { 
    Serial.println("true");
    buttonPressed = id;
    return true; 
  }
  
  Serial.println("false");
  return false;
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
  DisplayMessage(0, 0, "Kalibrierung");

  // read value from EEPROM
  unsigned int storedTotalTrackSteps = 0;
  EEPROM.get(0, storedTotalTrackSteps);

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

  // Goto second End Stop and record each step until enstop B is triggered
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

  // Now motor is at position totalTrackSteps
  // from now on we need to track every step movement in currentStepPosition variable
  currentStepPosition = totalTrackSteps;

  // move back a little 10% of the totalTrackSteps
  unsigned int tenPercentSteps = totalTrackSteps / 100 * 10;
  for (size_t i = 0; i < tenPercentSteps; i++)
  {
    MotorStep();
  }

  Serial.println("Calibration finished");
  Serial.print("Total track steps:");
  Serial.println(totalTrackSteps);

  // if new value differs from stored value then save it to EEPROM
  if ( storedTotalTrackSteps != totalTrackSteps )
  {
    EEPROM.put(0, totalTrackSteps);
  }

  DisplayMessage(0, 40, "Gespeichert");
  delay(2000);
}


/*****************************************************
* MotorModeSwitch()
* switches thorugh the different motor modes
*/
void MotorModeSwitch()
{
  motorMode++;
  if ( motorMode >= 2)
  {
    motorMode = 0;
  }
  Serial.print("motorMode: ");
  Serial.println(motorMode);
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

  if ( motorDirection == HIGH )
  {
    currentStepPosition++;
  }else{
    currentStepPosition--;
  }
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
  Serial.println(message);
  display.display();
}