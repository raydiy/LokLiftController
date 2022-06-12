#include <Arduino.h>
#include <Encoder.h>
#include <Bounce2.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROM.h>

/********** PINS MOTOR ***************************************************/
#define PIN_DRIVER_ENA 22 // ENA+ Pin
#define PIN_DRIVER_PUL 24 // PUL+ Pin
#define PIN_DRIVER_DIR 26 // DIR+ Pin
/*************************************************************************/

/********** PINS ROTARY ENCODER ******************************************/
#define PIN_ROTARY_ENCODER_CLK 27 // CLK
#define PIN_ROTARY_ENCODER_DT 25  // DT
#define PIN_ROTARY_ENCODER_SW 23  // Switch
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
SWITCH 13: 52 (MODUS SWITCH)
SWITCH 14: 23 (PIN_ROTARY_ENCODER_SW)
*/
#define NUM_BUTTONS 14
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {44, 46, 48, 50, 36, 38, 40, 42, 28, 30, 32, 34, 52, PIN_ROTARY_ENCODER_SW};
Bounce *buttons = new Bounce[NUM_BUTTONS];
/*************************************************************************/

/********** PINS END STOPS ***********************************************/
#define PIN_ENDSTOP_A 8 // Endstop switch pin
#define PIN_ENDSTOP_B 9 // Endstop switch pin
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
//int motorMinSpeed                       = 2000;     // base speed of the motor when in DRIVE MODE
//int motorMaxSpeed                       = 357;      // 357 is about 420 U/min
//int motorCalibrationSpeed               = 500;      // 500 is about 300 U/min, this speed is used during calibration
int buttonPressed                       = -1;       // the array ID of the button that has been pressed last
unsigned long totalTrackSteps           = 0;        // the number of steps from one end stop to the the other end stop
unsigned long startTime                 = 0;        // used for different situations where a startTime is needed
unsigned long currentStepPosition       = 0;        // the current position of the motor in steps
long encoderPosition                    = 0;        // the current encoder position
long oldEncoderPosition                 = 0;        // old encoder position (needed for reading encoder changes)
byte motorMode                          = 0;        // different motorModes: 1 continuos, 2 single step
unsigned long targetPositions[12];                  // holds the 12 stored positions loaded from EEPROM in steps

int motorPPR                            = 200;      // pulses per revolution of the motor, needed to caclulate the motor speed
int motorMinSpeedRPM                    = 25;      // minimum motor speed in rounds per minute
int motorMaxSpeedRPM                    = 420;      // maximum motor speed in rounds per minute
int motorCalibrationSpeedRPM            = 300;      // this speed is used during calibration in rpm
int motorPulseDelay                     = 2000;     // the pulse delay we use in the MotorStep() function
boolean motorDirection                  = LOW;      // LOW = clockwise rotation

/*************************************************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DECLARATIONS //////////////////////////////////////////////////////////////////////////////////
//
//
int CalculateEEPROMAddressForButton( byte buttonID );
bool CheckButton(byte id);
int CheckButtons();
bool CheckEndStopA();
bool CheckEndStopB();
void DebugPrintEEPROM();
int Delay2RPM( int delayValue );
void DisplayClear();
void DisplayMessage(int x, int y, String message);
void EncoderReset();
void LoadEEPROMData();
void MotorChangeDirection();
void MotorCalibrateEndStops();
void MotorSettings();
void MotorStep();
void MotorMoveTo( unsigned long targetPosition );
void MotorMoveToEndStopA();
void MotorModeSwitch();
void PrepareForMainLoop();
int RPM2Delay( int rpm );
void SavePosition();
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
    
    DisplayClear();
    DisplayMessage(20, 0, "LokLift");
    DisplayMessage(10, 10, "Controller");
    DisplayMessage(0, 30, "Starte");

    // Load Data from EEPROM
    //DebugPrintEEPROM();
    LoadEEPROMData();

    /* BUTTONS SETUP */

    // setup 14 button bounce objects
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        buttons[i].attach(BUTTON_PINS[i], INPUT_PULLUP); // setup the bounce instance for the current button
        buttons[i].interval(25);                         // interval in ms
    }

    /* SETUP ENDSTOP BUTTONS */

    endStopA.attach(PIN_ENDSTOP_A, INPUT_PULLUP);
    endStopA.interval(25);
    endStopB.attach(PIN_ENDSTOP_B, INPUT_PULLUP);
    endStopB.interval(25);

    /* MOTOR SETUP */

    pinMode(PIN_DRIVER_DIR, OUTPUT);
    pinMode(PIN_DRIVER_PUL, OUTPUT);

    // enable motor
    digitalWrite(PIN_DRIVER_ENA, LOW);

    // check five seconds for button presses during startup to enter configuration modes
    startTime = millis();
    while ( millis() < startTime + 5000 )
    { 
        unsigned long m = millis();
        if ( m > startTime && m < startTime + 1000 ) { DisplayMessage(40, 30, "."); }
        else if ( m > startTime && m < startTime + 2000 ) { DisplayMessage(40, 30, ".."); }
        else if ( m > startTime && m < startTime + 3000 ) { DisplayMessage(40, 30, "..."); }
        else if ( m > startTime && m < startTime + 4000 ) { DisplayMessage(40, 30, "...."); }
        else if ( m > startTime && m < startTime + 5000 ) { DisplayMessage(40, 30, "....."); }
        CheckButtons();

        // if button 12 is pressed on startup then start calibration
        if (buttonPressed == 12)
        {
            MotorCalibrateEndStops();
        }
        // if button 13 is pressed on startup then start motor setup
        else if (buttonPressed == 13)
        {
            MotorSettings();
        }
    }

    // reset last button pressed
    buttonPressed = -1;

    DisplayClear();
    DisplayMessage(20, 0, "LokLift");
    DisplayMessage(10, 10, "Controller");
    DisplayMessage(0, 30, "Kalibriere ...");

    MotorMoveToEndStopA();

    // Now motor is at position 0
    // from now on we need to track every step movement in currentStepPosition variable
    // which is handled in MotorStep(). so ALWAYS use MotorStep()
    currentStepPosition = 0;

    // move back a little 10% of the totalTrackSteps
    unsigned int tenPercentSteps = totalTrackSteps / 100 * 10;
    for (size_t i = 0; i < tenPercentSteps; i++)
    {
        MotorStep();
    }

    PrepareForMainLoop();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP ///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void loop()
{
    /* BUTTONS CHECK LOOP */
    CheckButtons();

    encoderPosition = rotaryEncoder.read();
    if (oldEncoderPosition != encoderPosition)
    {
        oldEncoderPosition = encoderPosition;
        Serial.println(encoderPosition);
    }

    // if button 1 to 12 is pressed
    // drive motor to position
    if ( buttonPressed >= 0 &&  buttonPressed <= 11 )
    {
        unsigned long targetPosition = 0;
        EEPROM.get( CalculateEEPROMAddressForButton(buttonPressed), targetPosition );
        MotorMoveTo( targetPosition );
        buttonPressed = -1;
    }

    // check for button 12 to initiate saving curent position
    else if (buttonPressed == 12)
    {
        SavePosition();
    }

    // check for rotay encoder knob switch press
    else if (buttonPressed == 13)
    {
        MotorModeSwitch();
        EncoderReset();
        buttonPressed = -1;
    }

    // MOTOR LOOP MODES

    // CONTINOUS MOTOR MODE / DRIVE MODE / LAUF-MODUS
    // rotary encoder controls the speed
    if (motorMode == 0)
    {
        if (encoderPosition != 0)
        {
            motorDirection = encoderPosition > 0 ? LOW : HIGH;
            motorPulseDelay = RPM2Delay( motorMinSpeedRPM ) - abs(encoderPosition * 100);
            if ( motorPulseDelay < RPM2Delay( motorMaxSpeedRPM ) ) { motorPulseDelay = RPM2Delay( motorMaxSpeedRPM ); }
            MotorStep();

            if ( CheckEndStopA() || CheckEndStopB() )
            {
                encoderPosition = -encoderPosition;
                oldEncoderPosition = -oldEncoderPosition;
                rotaryEncoder.write( encoderPosition );
            }
        }
    }

    // STEP MOTOR MODE / STEP MODE / SCHRITT-MODUS
    // each encoder step is a single motor step
    else if (motorMode == 1)
    {
        // Single Motor Step Mode
        if (encoderPosition != 0)
        {
            motorDirection = encoderPosition > 0 ? LOW : HIGH;
            MotorStep();
            EncoderReset();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS ///////////////////////////////////////////////////////////////////////////////////

/*****************************************************
 * CalculateEEPROMAddressForButton()
 * Caclualtes the address for a certain position in EEPROM
 * every value is an unsigned long
 * position 0 contains the whole track length in steps
 * position 1 to 12 correspondents to the switches 1 to 12 and stores 
 * the motor position in steps
 * 
 * byte buttonID must be a number between 0 and 11 and correspondents to the array position in the buttons Array 
 */
int CalculateEEPROMAddressForButton( byte buttonID )
{
    //unsigned int storedTotalTrackSteps = 0;
    //EEPROM.get(0, storedTotalTrackSteps);

    // add the size of the first value (track length)
    int result = sizeof(unsigned long);

    // now add the sizes of the switches before the current one
    result += buttonID * sizeof(unsigned long);

    return result;
}


/*****************************************************
 * RPM2Delay( int rpm )
 * Calculate the delay value in microseconds we need to use in the MotorStep()
 * function for the the rounds per minute
 * 
 * int rpm - the rounds pe rminute value we are aiming at
 */
int RPM2Delay( int rpm )
{
    return (60000000 / rpm) / motorPPR;
}

/*****************************************************
 * Delay2RPM( int delayValue )
 * Calculate the rounds per minute for a pulse delay value we use in
 * MotorStep() function
 * 
 * int delayValue - the pulse delay value we want to calculate into rounds per minute
 */
int Delay2RPM( int delayValue )
{
    return (60 / (delayValue / 1000000)) / motorPPR;
}


/*****************************************************
 * CheckButtons()
 */
int CheckButtons()
{
    for (int i = 0; i < NUM_BUTTONS; i++)
    {
        // Update the Bounce instance
        buttons[i].update();

        // If button has been pressed
        if (buttons[i].fell())
        {
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

    if (buttons[id].fell())
    {
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

    if (endStopA.fell())
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

    if (endStopB.fell())
    {
        Serial.println("endStopB betaetigt");
        return true;
    }

    return false;
}


/*****************************************************
 * EncoderReset()
 * Resets the encoder position to zero
 */
void EncoderReset()
{
    oldEncoderPosition = 0;
    encoderPosition = 0;
    rotaryEncoder.write(0);
}


/*****************************************************
 * LoadEEPROMData()
 * Loads the datat from EEPROM to the appropriate variables
 */
void LoadEEPROMData()
{
    Serial.println("LoadEEPROMData()");

    int address = 0;
    EEPROM.get(address, totalTrackSteps);
    address += sizeof(totalTrackSteps);

    Serial.print("totalTrackSteps: ");
    Serial.println(totalTrackSteps);

    for (size_t i = 0; i < 12; i++)
    {
        EEPROM.get(address, targetPositions[i]);
        address += sizeof(targetPositions[i]);

        Serial.print("targetPositions[");
        Serial.print(i);
        Serial.print("]: ");
        Serial.println(targetPositions[i]);
    }
}


/*****************************************************
 * MotorChangeDirection()
 * Changes the direction of the motor
 */
void MotorChangeDirection()
{
    Serial.println("MotorChangeDirection()");
    motorDirection = !motorDirection;
}

/*****************************************************
 *  MotorCalibrateEndStops()
 */
void MotorCalibrateEndStops()
{
    Serial.println("MotorCalibrateEndStops()");

    DisplayClear();
    DisplayMessage(0, 0, "Strecke messen");

    // read value from EEPROM
    unsigned int storedTotalTrackSteps = 0;
    EEPROM.get(0, storedTotalTrackSteps);

    bool hasFirstEndStopTriggered = false;
    bool hasSecondEndStopTriggered = false;

    // set the speed of the motor to calibrationSpeed
    motorPulseDelay = RPM2Delay( motorCalibrationSpeedRPM );

    // set direction to move to EndStop B
    motorDirection = LOW;

    // Goto first End Stop B
    while (hasFirstEndStopTriggered == false)
    {
        MotorStep();
        if ( CheckEndStopB() == true )
        {
            MotorChangeDirection();
            hasFirstEndStopTriggered = true;
            DisplayMessage(0, 10, "Endstop B");
        }
    }

    // Goto End Stop A and record each step until enstop A is triggered
    while (hasSecondEndStopTriggered == false)
    {
        MotorStep();
        totalTrackSteps++;

        if ( CheckEndStopA() == true )
        {
            MotorChangeDirection();
            hasSecondEndStopTriggered = true;
            DisplayMessage(0, 20, "Endstop A");
            DisplayMessage(0, 30, String(totalTrackSteps));
        }
    }

    // Now motor is at position 0
    // from now on we need to track every step movement in currentStepPosition variable
    currentStepPosition = 0;

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
    if (storedTotalTrackSteps != totalTrackSteps)
    {
        EEPROM.put(0, totalTrackSteps);
    }

    // debug dump EEPROM to console
    DebugPrintEEPROM();

    DisplayMessage(0, 40, "Gespeichert");
    delay(2000);
}


/*****************************************************
 * MotorModeSwitch()
 * switches through the different motor modes
 */
void MotorModeSwitch()
{
    motorMode++;
    if (motorMode >= 2)
    {
        motorMode = 0;
    }

    DisplayClear();

    if ( motorMode == 0 )
    {
        DisplayMessage( 0,0, "Lauf-Modus");
    }
    else if ( motorMode == 1 )
    {
        DisplayMessage( 0,0, "Schritt-Modus");
    }

    Serial.print("motorMode: ");
    Serial.println(motorMode);

    delay(1500);

    // back to main loop
    PrepareForMainLoop();
}


/*****************************************************
 *  MotorSettings()
 */
void MotorSettings()
{
    Serial.println("MotorSettings()");

    DisplayClear();
    DisplayMessage(0, 0, "Motor Setup");
}


/*****************************************************
 * MotorMoveTo( unsigned long targetPosition )
 * moves the motor until target position is met
 */
void MotorMoveTo( unsigned long targetPosition )
{
    Serial.print("MotorMoveTo() targetPosition: ");
    Serial.println(targetPosition);

    DisplayClear();
    DisplayMessage(0,0, "Bahn frei!");
    DisplayMessage(0,20, "Ziel: ");
    DisplayMessage(0,30, String(targetPosition));

    // cancel if target position is 0 or 4294967295 which is
    // the max value for unsigned long
    if ( targetPosition == 0 || targetPosition == 4294967295 )
    {
        Serial.println("CANCELLED: it seems no position has been saved to the last pressed button, yet.");
        return;
    } 

    unsigned long stepsNeeded = abs( (long)currentStepPosition - (long)targetPosition );
    unsigned long stepsDone = 0;
    unsigned long stepsLeft = stepsNeeded;

    // set motor speed
    int maxMotorPulseDelay      = RPM2Delay( motorMaxSpeedRPM );
    int minMotorPulseDelay      = RPM2Delay( motorMinSpeedRPM );
    int startMotorPulseDelay    = 15000;    // the starting speed delay
    int accelLength             = 200;      // the number of steps for the acceleration phase
    // the delay decrease per step during accellaration phase
    int accelDelay              = abs(startMotorPulseDelay - maxMotorPulseDelay)/accelLength;

    // Serial.print("stepsNeeded: ");
    // Serial.println(stepsNeeded);
    // Serial.print("maxMotorPulseDelay: ");
    // Serial.println(maxMotorPulseDelay);
    // Serial.print("minMotorPulseDelay: ");
    // Serial.println(minMotorPulseDelay);

    // move motor as long as target is not reached
    while( currentStepPosition != targetPosition )
    {
        motorPulseDelay = maxMotorPulseDelay;

        if (stepsDone < accelLength )
        {
            motorPulseDelay = startMotorPulseDelay - (accelDelay * stepsDone);
        }

        if ( stepsDone > stepsNeeded - accelLength )
        {
            motorPulseDelay = maxMotorPulseDelay + (accelDelay * (accelLength-stepsLeft));
        }

        // always cap the max motor speed delay to maxMotorPulseDelay
        if ( motorPulseDelay < maxMotorPulseDelay ) { motorPulseDelay = maxMotorPulseDelay; }

        // set the correct direction to reach the target position
        motorDirection = currentStepPosition < targetPosition ? LOW : HIGH;
        MotorStep();
        stepsDone++;
        stepsLeft--;

        if( CheckEndStopA() || CheckEndStopB() )
        {
            MotorChangeDirection();
        }
        
        //Serial.print("stepsDone: ");
        //Serial.println(stepsDone);
        //Serial.print("motorPulseDelay: ");
        //Serial.println(motorPulseDelay);
        //Serial.print(currentStepPosition);
        //Serial.print("/");
        //Serial.println(targetPosition);
    }

    DisplayMessage(0,40, "Fertig!");
    delay(1000);

    PrepareForMainLoop();
}

/*****************************************************
 * MotorMoveToEndStopA()
 * moves the motor until endstop A is triggered
 */
void MotorMoveToEndStopA()
{
    motorDirection = HIGH;
    motorPulseDelay = RPM2Delay( motorCalibrationSpeedRPM );

    while( CheckEndStopA() == false )
    {
        MotorStep();
    }
    MotorChangeDirection();
}


/*****************************************************
 * MotorStep()
 * do a single motor step
 */
void MotorStep()
{
    digitalWrite(PIN_DRIVER_DIR, motorDirection);
    digitalWrite(PIN_DRIVER_PUL, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_DRIVER_PUL, LOW);
    delayMicroseconds(motorPulseDelay - 20);

    if (motorDirection == HIGH)
    {
        currentStepPosition--;
    }
    else
    {
        currentStepPosition++;
    }

    //Serial.print("currentStepPosition: ");
    //Serial.println(currentStepPosition);
}


/*****************************************************
 * PrepareForMainLoop()
 * Prepares the display and other stuff to go back from sub loops to the main loop
 */
void PrepareForMainLoop()
{
    DisplayClear();
    DisplayMessage(0, 0, "Bahn frei!");
    DisplayMessage(0, 20, "Position:");
    DisplayMessage(0, 30, String(currentStepPosition));

    buttonPressed = -1;
    EncoderReset();
}


/*****************************************************
 * UpdateDisplay()
 */
void UpdateDisplay()
{
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(BLACK);

    display.setCursor(0, 0);
    display.println("Position: ");
    display.setCursor(0, 100);
    display.println( String(currentStepPosition ));

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
    display.setCursor(x, y);
    display.println(message);
    Serial.println(message);
    display.display();
}


/*****************************************************
 * SavePosition
 * Aks the user to save the current position to EEPROM
 */
void SavePosition()
{
    // reset the encoder to position zero and 
    // resetthe last button press
    EncoderReset();
    buttonPressed = -1;

    // display a message
    DisplayClear();
    DisplayMessage(0, 0, "Position");
    DisplayMessage(0, 10, "Speichern?");
    DisplayMessage(0, 20, String(currentStepPosition));
    
    // loop until a button is pressed to store the current positon to
    // or to cancel storing the position
    while (buttonPressed == -1)
    {
        // check for button press
        CheckButtons();
        if ( buttonPressed >= 0 && buttonPressed <= 11 )
        {
            // store position
            EEPROM.put( CalculateEEPROMAddressForButton(buttonPressed), currentStepPosition );

            // display message
            DisplayMessage(0, 30, "Gespeichert");
            DisplayMessage(0, 40, "Schalter: ");
            DisplayMessage(60, 40, String(buttonPressed+1));
            delay(2000);
        }

        // if button 12 or 13 is pressed: cancel
        else if (buttonPressed >= 12)
        {
            // cancel save
            DisplayMessage(0, 30, "Abbruch");
            delay(2000);
        }
    }

    // initialise displayreset last buttonPressed 
    // and back to main loop
    PrepareForMainLoop();

    // Dump EEPROM to console 
    DebugPrintEEPROM();
}


/*****************************************************
 * DebugPrintEEPROM
 * Print the EEPROM data to console
 */
void DebugPrintEEPROM()
{
    unsigned int data = 0;
    Serial.println( "Debug Print EEPROM:" );

    EEPROM.get( 0, data );
    Serial.print( "trackLenght: " );
    Serial.println( data );

    for (size_t i = 0; i <= 11; i++)
    {
        EEPROM.get( CalculateEEPROMAddressForButton(i), data );
        Serial.print( "Switch " );
        Serial.print( i );
        Serial.print( ": " );
        Serial.println( data );
    }
}