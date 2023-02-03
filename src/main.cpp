#include <Arduino.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
#include <Bounce2.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <EEPROM.h>
#include <AccelStepper.h>

/********** PINS MOTOR ***************************************************/
#define PIN_DRIVER_ENA 22 // ENA+ Pin
#define PIN_DRIVER_PUL 24 // PUL+ Pin
#define PIN_DRIVER_DIR 26 // DIR+ Pin
AccelStepper stepper(AccelStepper::DRIVER, PIN_DRIVER_PUL, PIN_DRIVER_DIR);
/*************************************************************************/

/********** PINS ROTARY ENCODER ******************************************/
#define PIN_ROTARY_ENCODER_CLK 27 // CLK
#define PIN_ROTARY_ENCODER_DT 25  // DT
#define PIN_ROTARY_ENCODER_SW 23  // Switch
// erzeuge ein neues Encoder Objekt
static ClickEncoder rotaryEncoder(PIN_ROTARY_ENCODER_DT, PIN_ROTARY_ENCODER_CLK, PIN_ROTARY_ENCODER_SW, 2, LOW);
static TimerOne timer;
/*************************************************************************/

/********** PINS SWITCHES ************************************************/
// SWITCH 1: 44    SWITCH 7: 40
// SWITCH 2: 46    SWITCH 8: 42
// SWITCH 3: 48    SWITCH 9: 28
// SWITCH 4: 50    SWITCH 10: 30
// SWITCH 5: 36    SWITCH 11: 32
// SWITCH 6: 38    SWITCH 12: 34
// SWITCH 13: 52 (MODUS SWITCH)
// SWITCH 14: 23 (PIN_ROTARY_ENCODER_SW)
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
int BUTTON_PRESSED                          = -1;       // the array ID of the button that has been pressed last
unsigned long TOTAL_TRACK_STEPS             = 0;        // [EEPROM] the number of steps from one end stop to the the other end stop
unsigned long START_TIME                    = 0;        // used for different situations where a START_TIME is needed
int16_t ENCODER_CHANGE                      = 0;        // the current encoder change value
long TARGET_POSITIONS[12];                              // [EEPROM] holds the 12 stored positions loaded from EEPROM in steps
byte MOTOR_MODE                             = 0;        // different motorModes: 1 continuos, 2 single step

// NEW AccelStepper Settings
float MAX_SPEED                             = 800.0;    // [EEPROM] the maximum motor speed setting
float ACCELERATION                          = 150.0;    // [EEPROM] the acceleration value
float CALIBRATION_MAX_SPEED                 = 200.0;    // [EEPROM] the maximum motor speed during calibration

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
void DisplayClear();
void DisplayMessage(int x, int y, String message, bool inverted=false);
void DrawMotorSettings( byte selectedCol, byte selectedRow );
void EncoderReset();
void InterruptTimerCallback();
void LoadEEPROMData();
void MeasureTrackLength();
void MotorSettings();
void MotorMoveTo( long targetPosition );
void MotorMoveToEndStopA();
void MotorModeSwitch();
void PrepareForMainLoop();
void SavePosition();
void SaveMotorSettings();
void UpdateDisplay();
void UpdateMainLoop_MotorModes();
void UpdateMainLoop_ButtonChecks();


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
    DisplayMessage(0, 25, "Starte");
    display.drawChar(0, 40, 0x2A, BLACK, WHITE, 1);
    display.drawChar(78, 40, 0x12, BLACK, WHITE, 1);

    // Load Data from EEPROM
    //DebugPrintEEPROM();
    LoadEEPROMData();

    /* ROTARY ENCODER SETUP */

    rotaryEncoder.setAccelerationEnabled(true);
    rotaryEncoder.setDoubleClickEnabled(true);
    rotaryEncoder.setLongPressRepeatEnabled(false);

    /* Interrupt Timer Setup */

    timer.initialize(1000);
    timer.attachInterrupt(InterruptTimerCallback); 

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

    stepper.setMaxSpeed(MAX_SPEED);
    stepper.setAcceleration(ACCELERATION);
    stepper.setSpeed(0);

    // check five seconds for button presses during startup to enter configuration modes
    bool gotoMeasureTrackLength = false;
    bool gotoMotorSettings = false;

    START_TIME = millis();
    while ( millis() < START_TIME + 5000 )
    { 
        unsigned long m = millis();
        if ( m > START_TIME && m < START_TIME + 1000 ) { DisplayMessage(40, 25, "."); }
        else if ( m > START_TIME && m < START_TIME + 2000 ) { DisplayMessage(40, 25, ".."); }
        else if ( m > START_TIME && m < START_TIME + 3000 ) { DisplayMessage(40, 25, "..."); }
        else if ( m > START_TIME && m < START_TIME + 4000 ) { DisplayMessage(40, 25, "...."); }
        else if ( m > START_TIME && m < START_TIME + 5000 ) { DisplayMessage(40, 25, "....."); }
        CheckButtons();

        // if button 12 (red button) is pressed on startup then start calibration
        if (BUTTON_PRESSED == 12)
        {
            gotoMeasureTrackLength = true;
            break;
        }
        // if button 13 (rotary knob) is pressed on startup then start motor setup
        else if (BUTTON_PRESSED == 13)
        {
            gotoMotorSettings = true;
            break;
        }
    }

    // reset last button pressed
    BUTTON_PRESSED = -1;

    if ( gotoMeasureTrackLength ) { MeasureTrackLength(); }
    else if ( gotoMotorSettings ) { MotorSettings(); }

    // only start Calibrating Process, if we are not coming from MeasureTrackLength
    if ( gotoMeasureTrackLength == false )
    {
        DisplayClear();
        DisplayMessage(20, 0, "LokLift");
        DisplayMessage(10, 10, "Controller");
        DisplayMessage(0, 30, "Kalibriere ...");

        MotorMoveToEndStopA();

        // move back a little 10% of the TOTAL_TRACK_STEPS to not permanent press endstop A
        unsigned int tenPercentSteps = round(TOTAL_TRACK_STEPS / 100.0 * 10.0);
        stepper.setSpeed(0);
        stepper.runToNewPosition(TOTAL_TRACK_STEPS - tenPercentSteps);
    }

    Serial.print("Current Motor Position: ");
    Serial.println(stepper.currentPosition());

    PrepareForMainLoop();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP ///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void loop()
{

    /* CHECK BUTTON INPUTS*/

    UpdateMainLoop_ButtonChecks();

    /* MOTOR LOOP MODES */ 

    UpdateMainLoop_MotorModes();
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

    // add the size of the first value (track length)
    int result = sizeof(TOTAL_TRACK_STEPS);

    // now add the sizes of the switches before the current one
    result += buttonID * sizeof(TARGET_POSITIONS[0]);

    return result;
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
            BUTTON_PRESSED = i;
            return i;
        }
    }

    return -1;
}


/*****************************************************
 * CheckButton(byte id)
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
        BUTTON_PRESSED = id;
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
    Serial.println("EncoderReset()");
    rotaryEncoder.reset();
    ENCODER_CHANGE = 0;
}


/*****************************************************
 * LoadEEPROMData()
 * Loads the data from EEPROM to the appropriate variables
 */
void LoadEEPROMData()
{
    Serial.println("LoadEEPROMData()");

    int address = 0;

    // TOTAL_TRACK_STEPS
    EEPROM.get(address, TOTAL_TRACK_STEPS);
    Serial.print("    TOTAL_TRACK_STEPS: ");
    Serial.print(TOTAL_TRACK_STEPS);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(TOTAL_TRACK_STEPS);

    // TARGET_POSITIONS
    for (size_t i = 0; i < 12; i++)
    {
        EEPROM.get(address, TARGET_POSITIONS[i]);
        Serial.print("    TARGET_POSITIONS[");
        Serial.print(i);
        Serial.print("]: ");
        Serial.print(TARGET_POSITIONS[i]);
        Serial.print(" | ");
        Serial.println(address);
        address += sizeof(TARGET_POSITIONS[i]);
    }

    // MAX_SPEED
    EEPROM.get(address, MAX_SPEED);
    Serial.print("    MAX_SPEED: ");
    Serial.print(MAX_SPEED);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MAX_SPEED);

    // ACCELERATION
    EEPROM.get(address, ACCELERATION);
    Serial.print("    ACCELERATION: ");
    Serial.print(ACCELERATION);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(ACCELERATION);

    // CALIBRATION_MAX_SPEED
    EEPROM.get(address, CALIBRATION_MAX_SPEED);
    Serial.print("    CALIBRATION_MAX_SPEED: ");
    Serial.print(CALIBRATION_MAX_SPEED);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(CALIBRATION_MAX_SPEED);
}



/*****************************************************
 *  MeasureTrackLength()
 */
void MeasureTrackLength()
{
    Serial.println("MeasureTrackLength()");

    DisplayClear();
    DisplayMessage(0, 0, "Strecke messen");

    // read storedTotalTrackSteps value from EEPROM
    // to compare later with measured value if writing to EEPROM is necessary
    unsigned int storedTotalTrackSteps = 0;
    EEPROM.get(0, storedTotalTrackSteps);

    bool hasFirstEndStopTriggered = false;
    bool hasSecondEndStopTriggered = false;

    // set the speed of the motor to calibrationSpeed
    stepper.setMaxSpeed(CALIBRATION_MAX_SPEED);
    
    // set direction to move to EndStop B
    stepper.move(-99999999);

    // Goto first End Stop B
    while (hasFirstEndStopTriggered == false)
    {
        // move motor 
        stepper.run();

        if ( CheckEndStopB() == true )
        {
            hasFirstEndStopTriggered = true;
            TOTAL_TRACK_STEPS = 0;
            stepper.setCurrentPosition(0);
            DisplayMessage(0, 10, "Endstop B");
        }
    }

    // Goto End Stop A and record each step until endstop A is triggered
    stepper.move(500000);

    while (hasSecondEndStopTriggered == false)
    {
        // move motor 
        stepper.run();
        if ( CheckEndStopA() == true )
        {
            //MotorChangeDirection();
            hasSecondEndStopTriggered = true;
            TOTAL_TRACK_STEPS = stepper.currentPosition();
            DisplayMessage(0, 20, "Endstop A");
            DisplayMessage(0, 30, String(TOTAL_TRACK_STEPS));
        }
    }

    // move back a little 10% of the TOTAL_TRACK_STEPS
    unsigned int tenPercentSteps = round(TOTAL_TRACK_STEPS / 100.0 * 10.0);
    
    Serial.print("TOTAL_TRACK_STEPS: ");
    Serial.println(TOTAL_TRACK_STEPS);
    Serial.print("Current Motor Position: ");
    Serial.println(stepper.currentPosition());
    Serial.print("New Position 10%: ");
    Serial.println(tenPercentSteps);

    stepper.setSpeed(0);
    stepper.runToNewPosition(TOTAL_TRACK_STEPS - tenPercentSteps);
    
    Serial.print("Current Motor Position: ");
    Serial.println(stepper.currentPosition());

    Serial.println("Calibration finished");
    Serial.print("Total track steps:");
    Serial.println(TOTAL_TRACK_STEPS);

    // if new value differs from stored value then save it to EEPROM
    if (storedTotalTrackSteps != TOTAL_TRACK_STEPS)
    {
        EEPROM.put(0, TOTAL_TRACK_STEPS);
    }

    // debug dump EEPROM to console
    LoadEEPROMData();

    DisplayMessage(0, 40, "Gespeichert");
    delay(2000);
}


/*****************************************************
 * MotorModeSwitch()
 * switches through the different motor modes
 */
void MotorModeSwitch()
{
    MOTOR_MODE++;
    if (MOTOR_MODE >= 2)
    {
        MOTOR_MODE = 0;
    }

    DisplayClear();

    if ( MOTOR_MODE == 0 )
    {
        DisplayMessage( 0,0, "Lauf-Modus");
    }
    else if ( MOTOR_MODE == 1 )
    {
        DisplayMessage( 0,0, "Schritt-Modus");
    }

    Serial.print("MOTOR_MODE: ");
    Serial.println(MOTOR_MODE);

    stepper.setSpeed(0);

    delay(1500);

    // back to main loop
    PrepareForMainLoop();
}


/*****************************************************
 * DrawMotorSettings( byte selectedCol, byte selectedRow )
 * 
 * Draw the motor settings menu
 * selectedCol is the currently selected column of the menu
 * selectedRow is the currently selected row of the menu
 * With these two values we can select a cell in the menu
 * that can be drawn inverted
 */
void DrawMotorSettings( byte selectedCol, byte selectedRow )
{
    DisplayClear();

    boolean selection[2][3] = {
        {false, false, false},
        {false, false, false}
    };

    selection[selectedCol][selectedRow] = true;
    
    display.drawChar(78, 40, 0x1A, BLACK, WHITE, 1);

    DisplayMessage(0, 0, "MaxPPS:", selection[0][0]);
    DisplayMessage(0, 10, "Accel:", selection[0][1]);
    DisplayMessage(0, 20, "CalPPS:", selection[0][2]);

    DisplayMessage(45, 0, String(MAX_SPEED), selection[1][0]);
    DisplayMessage(45, 10, String(ACCELERATION), selection[1][1]);
    DisplayMessage(45, 20, String(CALIBRATION_MAX_SPEED), selection[1][2]);
}


/*****************************************************
 * MotorSettings()
 * Setup some values for the motor settings controlled 
 * with the rotary encoder
 */
void MotorSettings()
{
    byte selectedCol = 0;
    byte selectedRow = 0;
    EncoderReset();
    rotaryEncoder.setAccelerationEnabled(false);

    // store these values for later camparison to check
    // if writing to EEPROM is neccessery at all
    unsigned int oldMotorMaxSpeedRPM    = MAX_SPEED;
    unsigned int oldAcceleration        = ACCELERATION;
    unsigned int oldCalibrationMaxSpeed = CALIBRATION_MAX_SPEED;

    // Draw the menu
    DrawMotorSettings( selectedCol, selectedRow );

    while( true )
    {

        // First column is selected
        // here we can select the row with the encoder
        if ( selectedCol == 0 )
        {
            ENCODER_CHANGE = rotaryEncoder.getIncrement();

            if ( ENCODER_CHANGE != 0 )
            {
                Serial.print("ENCODER_CHANGE: ");
                Serial.println(ENCODER_CHANGE);

                if (selectedRow == 0 && ENCODER_CHANGE < 0){ selectedRow = 5;}
                selectedRow = selectedRow + ENCODER_CHANGE;
                if (selectedRow > 4){ selectedRow = 0;}

                Serial.print("selectedRow: ");
                Serial.println(selectedRow);

                DrawMotorSettings( selectedCol, selectedRow );
            }
        }

        // Second column is selected
        // here we can adjust the selected value
        else if ( selectedCol == 1 )
        {
            ENCODER_CHANGE = rotaryEncoder.getIncrement();

            if ( ENCODER_CHANGE != 0 )
            {
                // calculate a value based on acceleration (= ENCODER_CHANGE)
                // the higher ENCODER_CHANGE is, ther more gets added to the value
                int16_t calcValueChange = ENCODER_CHANGE * ENCODER_CHANGE;
                if (ENCODER_CHANGE < 0){ calcValueChange = -calcValueChange; }

                if ( selectedRow == 0 ){ 
                    MAX_SPEED = MAX_SPEED + calcValueChange;
                    MAX_SPEED = constrain(MAX_SPEED, 5, 1000);

                }
                else if ( selectedRow == 1 ){ 
                    ACCELERATION = ACCELERATION + calcValueChange;
                    ACCELERATION = constrain(ACCELERATION, 5, 1000);
                }
                else if ( selectedRow == 2 ){ 
                    CALIBRATION_MAX_SPEED = CALIBRATION_MAX_SPEED + calcValueChange;
                    if ( CALIBRATION_MAX_SPEED < 5 ){ CALIBRATION_MAX_SPEED = 1000; }
                    else if ( CALIBRATION_MAX_SPEED > 1000 ){ CALIBRATION_MAX_SPEED = 5; }
                }

                DrawMotorSettings( selectedCol, selectedRow );
            }
        }

        // Button Checks
        CheckButtons();

        // check for rotay encoder knob switch press
        if (BUTTON_PRESSED == 13)
        {
            selectedCol++;
            if ( selectedCol > 1 ){ selectedCol = 0; }

            if (selectedCol == 0) { rotaryEncoder.setAccelerationEnabled(false); }
            else if (selectedCol == 1) { rotaryEncoder.setAccelerationEnabled(true); }

            Serial.print("selectedCol: ");
            Serial.println(selectedCol);

            BUTTON_PRESSED = -1;

            EncoderReset();
            DrawMotorSettings( selectedCol, selectedRow );
        }

        // check for button 12 (red button) to initiate saving end exit motor settings
        else if (BUTTON_PRESSED == 12)
        {
            BUTTON_PRESSED = -1;

            // only if any of the values haave changed write to EEPROM
            if ( 
                oldMotorMaxSpeedRPM != MAX_SPEED || 
                oldAcceleration != ACCELERATION || 
                oldCalibrationMaxSpeed != CALIBRATION_MAX_SPEED
            )
            {
                SaveMotorSettings();
            }

            EncoderReset();
            rotaryEncoder.setAccelerationEnabled(true);
            break;
        }

    }
}


/*****************************************************
 * MotorMoveTo( unsigned long targetPosition )
 * moves the motor until target position is met
 */
void MotorMoveTo( long targetPosition )
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

    stepper.setAcceleration(ACCELERATION);
    stepper.moveTo( targetPosition );

    while( stepper.currentPosition() != targetPosition )
    {
        stepper.run();

        if( CheckEndStopA() || CheckEndStopB() )
        {
            stepper.setSpeed( -stepper.speed() );
        }
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
    Serial.println("MotorMoveToEndStopA");

    stepper.setMaxSpeed(CALIBRATION_MAX_SPEED);
    stepper.move(99999999);

    while( CheckEndStopA() == false )
    {
        stepper.run();
    }

    // Now motor is at position TOTAL_TRACK_STEPS
    // from now on we need to track every step movement in CURRENT_STEP_POSITION variable
    // which is handled in MotorStep(). so ALWAYS use MotorStep()
    stepper.setCurrentPosition(TOTAL_TRACK_STEPS);
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
    DisplayMessage(0, 30, String(stepper.currentPosition()));

    BUTTON_PRESSED = -1;
    EncoderReset();

    stepper.setMaxSpeed(MAX_SPEED);
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
    display.println( String(stepper.currentPosition()));

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
void DisplayMessage(int x, int y, String message, bool inverted)
{
    if (inverted )
    {
        display.setTextColor(WHITE, BLACK);
    }else{
        display.setTextColor(BLACK, WHITE);
    }

    display.setCursor(x, y);
    display.println(message);
    display.display();
}


/*****************************************************
 * SavePosition
 * Asks the user to save the current position to EEPROM
 */
void SavePosition()
{
    // reset the encoder to position zero and 
    // resetthe last button press
    EncoderReset();
    BUTTON_PRESSED = -1;

    // display a message
    DisplayClear();
    DisplayMessage(0, 0, "Position");
    DisplayMessage(0, 10, "Speichern?");
    DisplayMessage(0, 20, String(stepper.currentPosition()));
    
    // loop until a button is pressed to store the current positon to
    // or to cancel storing the position
    while (BUTTON_PRESSED == -1)
    {
        // check for button press
        CheckButtons();
        if ( BUTTON_PRESSED >= 0 && BUTTON_PRESSED <= 11 )
        {
            // store position
            EEPROM.put( CalculateEEPROMAddressForButton(BUTTON_PRESSED), stepper.currentPosition() );

            // display message
            DisplayMessage(0, 30, "Gespeichert");
            DisplayMessage(0, 40, "Schalter: ");
            DisplayMessage(60, 40, String(BUTTON_PRESSED+1));
            delay(2000);
        }

        // if button 12 or 13 is pressed: cancel
        else if (BUTTON_PRESSED >= 12)
        {
            // cancel save
            DisplayMessage(0, 30, "Abbruch");
            delay(2000);
        }
    }

    // initialise displayreset last BUTTON_PRESSED 
    // and back to main loop
    PrepareForMainLoop();

    // Dump EEPROM to console 
    LoadEEPROMData();
}


/*****************************************************
 * SaveMotorSettings
 * Saves the values from motor settings screen to EEPROM
 */
void SaveMotorSettings()
{
    Serial.println("SaveMotorSettings");

    // calculate adress of MOTOR_MIN_SPEED_RPM
    // the first value is TOTAL_TRACK_STEPS
    int address = sizeof(TOTAL_TRACK_STEPS);

    // then comes the 12 TARGET_POSITIONS
    address += sizeof(TARGET_POSITIONS[0]) * 12;

    // MAX_SPEED
    EEPROM.put( address, MAX_SPEED );
    Serial.print("    Saved MAX_SPEED: ");
    Serial.print(MAX_SPEED);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MAX_SPEED);

    // ACCELERATION
    EEPROM.put( address, ACCELERATION );
    Serial.print("    Saved ACCELERATION: ");
    Serial.print(ACCELERATION);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(ACCELERATION);

    // CALIBRATION_MAX_SPEED
    EEPROM.put( address, CALIBRATION_MAX_SPEED );
    Serial.print("    Saved CALIBRATION_MAX_SPEED: ");
    Serial.print(CALIBRATION_MAX_SPEED);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(CALIBRATION_MAX_SPEED);
}


void InterruptTimerCallback()
{
  // This is the Encoder's worker routine. It will physically read the hardware
  // and all most of the logic happens here. Recommended interval for this method is 1ms.
  rotaryEncoder.service();
}


/*****************************************************
 * UpdateMainLoop_ButtonChecks
 * Updates the different button checks in main loop
 */
void UpdateMainLoop_ButtonChecks()
{
    /* BUTTONS CHECK LOOP */

    CheckButtons();

    // if button 1 to 12 is pressed
    // drive motor to position
    if ( BUTTON_PRESSED >= 0 &&  BUTTON_PRESSED <= 11 )
    {
        long targetPosition = 0;
        EEPROM.get( CalculateEEPROMAddressForButton(BUTTON_PRESSED), targetPosition );

        // if target position is in the total track steps range
        // then move to that target 
        if ( targetPosition <= (long)TOTAL_TRACK_STEPS )
        {
            MotorMoveTo( targetPosition );
        }

        // display an error message if target is not in
        // total track range
        else
        {
            DisplayClear();
            DisplayMessage(0,0, "Target out of range!");
            delay(4000);
            PrepareForMainLoop();
        }
        BUTTON_PRESSED = -1;
    }

    // check for button 12 to initiate saving curent position
    else if (BUTTON_PRESSED == 12)
    {
        SavePosition();
    }

    // check for rotay encoder knob switch press
    // to switch motor mode
    else if (BUTTON_PRESSED == 13)
    {
        MotorModeSwitch();
        BUTTON_PRESSED = -1;
    }

    // check for double click on rotary encoder knob
    // if double click detected, opens the MotorSettings menu
    switch (rotaryEncoder.getButton())
    {
        case Button::DoubleClicked:
            Serial.println("Encoder double clicked");
            MotorSettings();
            PrepareForMainLoop();
            break;
        default:
            break;
    }
}


/*****************************************************
 * UpdateMainLoop_MotorModes
 * Updates the two different motor modes in main loop
 */
void UpdateMainLoop_MotorModes()
{
    // get encoder values
    ENCODER_CHANGE = rotaryEncoder.getIncrement();

    // CONTINOUS MOTOR MODE aka DRIVE MODE aka LAUF-MODUS
    // rotary encoder controls the speed
    if (MOTOR_MODE == 0)
    {
        if (ENCODER_CHANGE != 0)
        {
            stepper.setSpeed(stepper.speed() + ENCODER_CHANGE);
        }

        stepper.runSpeed();

        if ( CheckEndStopA() || CheckEndStopB() )
        {
            // change motor direction
            stepper.setSpeed( -stepper.speed() );
        }

    }

    // STEP MOTOR MODE aka STEP MODE aka SCHRITT-MODUS
    // each encoder step is a single motor step
    else if (MOTOR_MODE == 1)
    {
        // Single Motor Step Mode
        if (ENCODER_CHANGE != 0)
        {
            stepper.move( ENCODER_CHANGE );
            stepper.runSpeed();
        }
    }
}