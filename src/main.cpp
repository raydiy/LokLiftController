#include <Arduino.h>
#include <ClickEncoder.h>
#include <TimerOne.h>
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
unsigned long CURRENT_STEP_POSITION         = 0;        // the current position of the motor in steps
int16_t ENCODER_CHANGE                      = 0;        // the current encoder change value
int16_t ENCODER_VALUE                       = 0;        // the current accumulated encoder value
int16_t ENCODER_VALUE_OLD                   = 0;        // old encoder position (needed for reading encoder changes)
unsigned long TARGET_POSITIONS[12];                 // [EEPROM] holds the 12 stored positions loaded from EEPROM in steps

byte MOTOR_MODE                             = 0;        // different motorModes: 1 continuos, 2 single step
unsigned int MOTOR_PPR                      = 200;      // [EEPROM] pulses per revolution of the motor, needed to caclulate the motor speed
unsigned long MOTOR_PULSE_DELAY             = 2000;     // the pulse delay we use in the MotorStep() function = stepping speed
boolean MOTOR_DIRECTION                     = LOW;      // LOW = clockwise rotation
unsigned int MOTOR_MIN_SPEED_RPM            = 25;       // [EEPROM] minimum motor speed in rounds per minute
unsigned int MOTOR_MAX_SPEED_RPM            = 420;      // [EEPROM] maximum motor speed in rounds per minute
unsigned int MOTOR_CALIBRATION_SPEED_RPM    = 300;      // [EEPROM] this speed is used during calibration in rpm
unsigned int ACCEL_STEPS                    = 400;      // [EEPROM] the number of steps for the acceleration phase in a move

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
int Delay2RPM( int delayValue );
void DisplayClear();
void DisplayMessage(int x, int y, String message, bool inverted=false);
void DrawMotorSettings( byte selectedCol, byte selectedRow );
void EncoderReset();
void InterruptTimerCallback();
unsigned int LerpLinear(unsigned int from, unsigned int to, unsigned int deltaSteps);
long LinearMap(long ax, long aMin, long aMax, long bMin, long bMax);
void LoadEEPROMData();
void MotorChangeDirection();
void MotorCalibrateEndStops();
void MotorSettings();
void MotorStep();
void MotorMoveTo( unsigned long targetPosition );
void MotorMoveToEndStopA();
void MotorModeSwitch();
void PrepareForMainLoop();
unsigned int RPM2Delay( int rpm );
void SavePosition();
void SaveMotorSettings();
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

    pinMode(PIN_DRIVER_DIR, OUTPUT);
    pinMode(PIN_DRIVER_PUL, OUTPUT);

    // enable motor
    digitalWrite(PIN_DRIVER_ENA, LOW);

    // check five seconds for button presses during startup to enter configuration modes
    bool gotoMotorCalibrateEndStops = false;
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
            gotoMotorCalibrateEndStops = true;
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

    if ( gotoMotorCalibrateEndStops ) { MotorCalibrateEndStops(); }
    else if ( gotoMotorSettings ) { MotorSettings(); }

    DisplayClear();
    DisplayMessage(20, 0, "LokLift");
    DisplayMessage(10, 10, "Controller");
    DisplayMessage(0, 30, "Kalibriere ...");

    MotorMoveToEndStopA();

    // Now motor is at position 0
    // from now on we need to track every step movement in CURRENT_STEP_POSITION variable
    // which is handled in MotorStep(). so ALWAYS use MotorStep()
    CURRENT_STEP_POSITION = 0;

    // move back a little 10% of the TOTAL_TRACK_STEPS to not permanent press endstop A
    unsigned int tenPercentSteps = round(TOTAL_TRACK_STEPS / 100.0 * 10.0);
    
    Serial.print("tenPercentSteps: ");
    Serial.println(tenPercentSteps);
    Serial.print("TOTAL_TRACK_STEPS: ");
    Serial.println(TOTAL_TRACK_STEPS);

    for (size_t i = 0; i < tenPercentSteps; i++)
    {
        MotorStep();
    }

    Serial.print("CURRENT_STEP_POSITION: ");
    Serial.println(CURRENT_STEP_POSITION);

    PrepareForMainLoop();

    MOTOR_DIRECTION = HIGH;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP ///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void loop()
{
    /* BUTTONS CHECK LOOP */

    CheckButtons();

    // if button 1 to 12 is pressed
    // drive motor to position
    if ( BUTTON_PRESSED >= 0 &&  BUTTON_PRESSED <= 11 )
    {
        unsigned long targetPosition = 0;
        EEPROM.get( CalculateEEPROMAddressForButton(BUTTON_PRESSED), targetPosition );

        // if target position is in the total track steps range
        // then move to that target 
        if ( targetPosition <= TOTAL_TRACK_STEPS )
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


    /* MOTOR LOOP MODES */ 

    // get encoder values
    ENCODER_CHANGE = rotaryEncoder.getIncrement();

    // CONTINOUS MOTOR MODE aka DRIVE MODE aka LAUF-MODUS
    // rotary encoder controls the speed
    if (MOTOR_MODE == 0)
    {
        if (ENCODER_CHANGE != 0)
        {
            ENCODER_VALUE = rotaryEncoder.getAccumulate();

            //Serial.print("ENCODER_CHANGE ");
            //Serial.print(ENCODER_CHANGE);
            //Serial.print(" -> ");
            //Serial.println(ENCODER_VALUE);

            MOTOR_PULSE_DELAY = RPM2Delay( MOTOR_MIN_SPEED_RPM ) - abs(ENCODER_VALUE * 100);
            
            if ( ENCODER_CHANGE < 0 && ENCODER_VALUE_OLD >= 0 && ENCODER_VALUE < 0 )
            {
                MotorChangeDirection();
            }
            else if ( ENCODER_CHANGE > 0 && ENCODER_VALUE_OLD <= 0 && ENCODER_VALUE > 0 )
            {
                MotorChangeDirection();
            }
            
            ENCODER_VALUE_OLD = ENCODER_VALUE;

        }

        // Step the motor if ENCODER_VALUE is not 0

        if (ENCODER_VALUE != 0)
        {
            // cap the delay to MOTOR_MAX_SPEED_RPM
            // the samller the delay the faster the motor steps
            if ( MOTOR_PULSE_DELAY < RPM2Delay( MOTOR_MAX_SPEED_RPM ) ) { MOTOR_PULSE_DELAY = RPM2Delay( MOTOR_MAX_SPEED_RPM ); }
            
            MotorStep();

            // TODO: Maybe soft motor direction change? Since we know how far we are away from endstopps, this could be possible

            if ( CheckEndStopA() || CheckEndStopB() )
            {
                MotorChangeDirection();
            }
        }
    }

    // STEP MOTOR MODE aka STEP MODE aka SCHRITT-MODUS
    // each encoder step is a single motor step
    else if (MOTOR_MODE == 1)
    {
        // Single Motor Step Mode
        if (ENCODER_CHANGE != 0)
        {
            MOTOR_DIRECTION = ENCODER_CHANGE > 0 ? LOW : HIGH;
            MotorStep();
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
unsigned int RPM2Delay( int rpm )
{
    return (60000000 / rpm) / MOTOR_PPR;
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
    return (60 / (delayValue / 1000000)) / MOTOR_PPR;
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
    ENCODER_VALUE = 0;
    ENCODER_VALUE_OLD = 0;
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

    // MOTOR_MIN_SPEED_RPM
    EEPROM.get(address, MOTOR_MIN_SPEED_RPM);
    Serial.print("    MOTOR_MIN_SPEED_RPM: ");
    Serial.print(MOTOR_MIN_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_MIN_SPEED_RPM);

    // MOTOR_MAX_SPEED_RPM
    EEPROM.get(address, MOTOR_MAX_SPEED_RPM);
    Serial.print("    MOTOR_MAX_SPEED_RPM: ");
    Serial.print(MOTOR_MAX_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_MAX_SPEED_RPM);

    // MOTOR_CALIBRATION_SPEED_RPM
    EEPROM.get(address, MOTOR_CALIBRATION_SPEED_RPM);
    Serial.print("    MOTOR_CALIBRATION_SPEED_RPM: ");
    Serial.print(MOTOR_CALIBRATION_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_CALIBRATION_SPEED_RPM);

    // ACCEL_STEPS
    EEPROM.get(address, ACCEL_STEPS);
    Serial.print("    ACCEL_STEPS: ");
    Serial.print(ACCEL_STEPS);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(ACCEL_STEPS);

    // MOTOR_PPR
    EEPROM.get(address, MOTOR_PPR);
    Serial.print("    MOTOR_PPR: ");
    Serial.print(MOTOR_PPR);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_PPR);
}


/*****************************************************
 * MotorChangeDirection()
 * Changes the direction of the motor
 */
void MotorChangeDirection()
{
    //Serial.println("MotorChangeDirection()");
    MOTOR_DIRECTION = !MOTOR_DIRECTION;
}


/*****************************************************
 *  MotorCalibrateEndStops()
 */
void MotorCalibrateEndStops()
{
    Serial.println("MotorCalibrateEndStops()");

    DisplayClear();
    DisplayMessage(0, 0, "Strecke messen");

    // read storedTotalTrackSteps value from EEPROM
    unsigned int storedTotalTrackSteps = 0;
    EEPROM.get(0, storedTotalTrackSteps);

    bool hasFirstEndStopTriggered = false;
    bool hasSecondEndStopTriggered = false;

    // set the speed of the motor to calibrationSpeed
    int calibrationMotorPulseDelay = RPM2Delay( MOTOR_CALIBRATION_SPEED_RPM );
    
    // set direction to move to EndStop B
    MOTOR_DIRECTION = LOW;

    // Goto first End Stop B
    int stepsDone = 0;
    while (hasFirstEndStopTriggered == false)
    {
        // add accleration
        MOTOR_PULSE_DELAY = LerpLinear(15000, calibrationMotorPulseDelay, stepsDone);
        MotorStep();
        stepsDone++;

        if ( CheckEndStopB() == true )
        {
            MotorChangeDirection();
            hasFirstEndStopTriggered = true;
            TOTAL_TRACK_STEPS = 0;
            DisplayMessage(0, 10, "Endstop B");

        }
    }

    // Goto End Stop A and record each step until endstop A is triggered
    stepsDone = 0;
    while (hasSecondEndStopTriggered == false)
    {
        // add accleration
        MOTOR_PULSE_DELAY = LerpLinear(15000, calibrationMotorPulseDelay, stepsDone);
        MotorStep();
        stepsDone++;

        TOTAL_TRACK_STEPS++;

        if ( CheckEndStopA() == true )
        {
            MotorChangeDirection();
            hasSecondEndStopTriggered = true;
            DisplayMessage(0, 20, "Endstop A");
            DisplayMessage(0, 30, String(TOTAL_TRACK_STEPS));
        }
    }

    // Now motor is at position 0
    // from now on we need to track every step movement in CURRENT_STEP_POSITION variable
    // this handles MotorStep(), so only use MotorStep() from now on
    CURRENT_STEP_POSITION = 0;

    // move back a little 10% of the TOTAL_TRACK_STEPS
    stepsDone = 0;
    unsigned int tenPercentSteps = TOTAL_TRACK_STEPS / 100 * 10;
    for (size_t i = 0; i < tenPercentSteps; i++)
    {
        // add accleration
        MOTOR_PULSE_DELAY = LerpLinear(15000, calibrationMotorPulseDelay, stepsDone);
        MotorStep();
        stepsDone++;
    }

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
        MOTOR_DIRECTION = HIGH;
    }
    else if ( MOTOR_MODE == 1 )
    {
        DisplayMessage( 0,0, "Schritt-Modus");
    }

    Serial.print("MOTOR_MODE: ");
    Serial.println(MOTOR_MODE);

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

    boolean selection[2][5] = {
        {false, false, false, false, false},
        {false, false, false, false, false}
    };

    selection[selectedCol][selectedRow] = true;
    
    display.drawChar(78, 40, 0x1A, BLACK, WHITE, 1);

    DisplayMessage(0, 0, "MaxRPM:", selection[0][0]);
    DisplayMessage(0, 10, "MinRPM:", selection[0][1]);
    DisplayMessage(0, 20, "CalRPM:", selection[0][2]);
    DisplayMessage(0, 30, "AccStp:", selection[0][3]);
    DisplayMessage(0, 40, "MotPPR:", selection[0][4]);

    DisplayMessage(45, 0, String(MOTOR_MAX_SPEED_RPM), selection[1][0]);
    DisplayMessage(45, 10, String(MOTOR_MIN_SPEED_RPM), selection[1][1]);
    DisplayMessage(45, 20, String(MOTOR_CALIBRATION_SPEED_RPM), selection[1][2]);
    DisplayMessage(45, 30, String(ACCEL_STEPS), selection[1][3]);
    DisplayMessage(45, 40, String(MOTOR_PPR), selection[1][4]);
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

    // store these values for later camparisons to check
    // if writing to EEPROM is neccessery at all
    unsigned int oldMotorMinSpeedRPM            = MOTOR_MIN_SPEED_RPM;
    unsigned int oldMotorMaxSpeedRPM            = MOTOR_MAX_SPEED_RPM;
    unsigned int oldMotorCalibrationSpeedRPM    = MOTOR_CALIBRATION_SPEED_RPM;
    unsigned int oldAccelSteps                  = ACCEL_STEPS;
    unsigned int oldMotorPPR                    = MOTOR_PPR;

    // Draw the menu
    DrawMotorSettings( selectedCol, selectedRow );

    while( true )
    {

        // First column is selected
        // here we can select the row with the encoder
        if ( selectedCol == 0 )
        {
            ENCODER_CHANGE = rotaryEncoder.getIncrement();
            ENCODER_VALUE = rotaryEncoder.getAccumulate();

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
            //ENCODER_VALUE = rotaryEncoder.getAccumulate();

            if ( ENCODER_CHANGE != 0 )
            {
                // calculate a value based on acceleration (= ENCODER_CHANGE)
                // the higher ENCODER_CHANGE is, ther more gets added to the value
                int16_t calcValueChange = ENCODER_CHANGE * ENCODER_CHANGE;
                if (ENCODER_CHANGE < 0){ calcValueChange = -calcValueChange; }

                if ( selectedRow == 0 ){ 
                    MOTOR_MAX_SPEED_RPM = MOTOR_MAX_SPEED_RPM + calcValueChange;
                    MOTOR_MAX_SPEED_RPM = constrain(MOTOR_MAX_SPEED_RPM, 5, 1000);

                }
                else if ( selectedRow == 1 ){ 
                    MOTOR_MIN_SPEED_RPM = MOTOR_MIN_SPEED_RPM + calcValueChange;
                    MOTOR_MIN_SPEED_RPM = constrain(MOTOR_MIN_SPEED_RPM, 5, 1000);
                }
                else if ( selectedRow == 2 ){ 
                    MOTOR_CALIBRATION_SPEED_RPM = MOTOR_CALIBRATION_SPEED_RPM + calcValueChange;
                    //MOTOR_CALIBRATION_SPEED_RPM = constrain(MOTOR_CALIBRATION_SPEED_RPM, 5, 1000);
                    if ( MOTOR_CALIBRATION_SPEED_RPM < 5 ){ MOTOR_CALIBRATION_SPEED_RPM = 1000; }
                    else if ( MOTOR_CALIBRATION_SPEED_RPM > 1000 ){ MOTOR_CALIBRATION_SPEED_RPM = 5; }
                }
                else if ( selectedRow == 3 ){ 
                    ACCEL_STEPS = ACCEL_STEPS + calcValueChange;
                    ACCEL_STEPS = constrain(ACCEL_STEPS, 0, 2000);
                }
                else if ( selectedRow == 4 ){ 
                    MOTOR_PPR = MOTOR_PPR + calcValueChange;
                    MOTOR_PPR = constrain(MOTOR_PPR, 100, 2000);
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
                oldMotorMinSpeedRPM != MOTOR_MIN_SPEED_RPM || 
                oldMotorMaxSpeedRPM != MOTOR_MAX_SPEED_RPM || 
                oldMotorCalibrationSpeedRPM != MOTOR_CALIBRATION_SPEED_RPM ||
                oldAccelSteps != ACCEL_STEPS ||
                oldMotorPPR != MOTOR_PPR
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

    unsigned long stepsNeeded = abs( (long)CURRENT_STEP_POSITION - (long)targetPosition );
    unsigned long stepsDone = 0;
    unsigned long stepsDoneA = 0;
    unsigned long stepsDoneB = 0;
    unsigned long stepsLeft = stepsNeeded;
    bool isDecelarating = false;
    
    unsigned int accelerationSteps = ACCEL_STEPS;
    if ( accelerationSteps * 2 > stepsNeeded )
    {
        accelerationSteps = round((float)stepsNeeded/2.0);
    }

    // set motor speed
    unsigned int maxMotorPulseDelay      = RPM2Delay( MOTOR_MAX_SPEED_RPM );
    unsigned int startMotorPulseDelay    = 15000;    // the starting speed delay, should be quite high to start slow (should this be adjustable? EEPROM candidate?)
    
    // the delay decrease per step during accellaration phase
    //unsigned int accelDelay     = abs(startMotorPulseDelay - maxMotorPulseDelay)/ACCEL_STEPS;

    // Serial.print("stepsNeeded: ");
    // Serial.println(stepsNeeded);
    // Serial.print("maxMotorPulseDelay: ");
    // Serial.println(maxMotorPulseDelay);
    // Serial.print("minMotorPulseDelay: ");
    // Serial.println(minMotorPulseDelay);

    // move motor as long as target is not reached
    while( CURRENT_STEP_POSITION != targetPosition )
    {
        // ACCELERATION PHASE
        if ( stepsDone <= accelerationSteps && isDecelarating == false )
        {
            MOTOR_PULSE_DELAY = LerpLinear(startMotorPulseDelay, maxMotorPulseDelay, stepsDoneA);

            // always cap the max motor speed delay to maxMotorPulseDelay
            // the smaller the delay, the faster the motor steps, that is why we use < (not >)
            if ( MOTOR_PULSE_DELAY < maxMotorPulseDelay ) { MOTOR_PULSE_DELAY = maxMotorPulseDelay; }
            
            stepsDoneA++;
        }

        // DECELERATION PHASE
        else if ( stepsLeft <= accelerationSteps )
        {
            if ( isDecelarating == false )
            {
                stepsDoneB = accelerationSteps;
                isDecelarating = true;
            }

            MOTOR_PULSE_DELAY = LerpLinear(startMotorPulseDelay, maxMotorPulseDelay, stepsDoneB);
            if ( MOTOR_PULSE_DELAY > startMotorPulseDelay ) { MOTOR_PULSE_DELAY = startMotorPulseDelay; }
            stepsDoneB--;
        }

        // set the correct direction to reach the target position
        MOTOR_DIRECTION = CURRENT_STEP_POSITION < targetPosition ? LOW : HIGH;

        MotorStep();
        stepsDone++;
        stepsLeft--;

        if( CheckEndStopA() || CheckEndStopB() )
        {
            MotorChangeDirection();
        }
        
        //Serial.print("stepsDone: ");
        //Serial.println(stepsDone);
        //Serial.print("MOTOR_PULSE_DELAY: ");
        //Serial.println(MOTOR_PULSE_DELAY);
        //Serial.print(CURRENT_STEP_POSITION);
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
    Serial.println("MotorMoveToEndStopA");

    // endstop A should be in counter clock wise motor rotation
    MOTOR_DIRECTION = HIGH;

    // set motor speed
    unsigned int maxMotorPulseDelay     = RPM2Delay( MOTOR_CALIBRATION_SPEED_RPM );
    unsigned long startMotorPulseDelay   = 150000;
    unsigned long stepsDone             = 0;

    Serial.print("MOTOR_CALIBRATION_SPEED_RPM: ");
    Serial.println(MOTOR_CALIBRATION_SPEED_RPM);
    Serial.print("Converted to delay time with RPM2Delay -> ");
    Serial.print("maxMotorPulseDelay: ");
    Serial.println(maxMotorPulseDelay);
    Serial.print("startMotorPulseDelay: ");
    Serial.println(startMotorPulseDelay);
    
    while( CheckEndStopA() == false )
    {
        //MOTOR_PULSE_DELAY = LerpLinear(startMotorPulseDelay, maxMotorPulseDelay, stepsDone);
        MOTOR_PULSE_DELAY = LinearMap(stepsDone, 0, ACCEL_STEPS, startMotorPulseDelay, maxMotorPulseDelay);

        /*
        if ( stepsDone <= ACCEL_STEPS )
        {
            Serial.print(stepsDone);
            Serial.print("# MOTOR_PULSE_DELAY: ");
            Serial.println(MOTOR_PULSE_DELAY);
        }
        */
        
        MotorStep();
        stepsDone++;
    }
    MotorChangeDirection();
}


/*****************************************************
 * MotorStep()
 * do a single motor step
 */
void MotorStep()
{
    digitalWrite(PIN_DRIVER_DIR, MOTOR_DIRECTION);
    digitalWrite(PIN_DRIVER_PUL, HIGH);
    delayMicroseconds(20);
    digitalWrite(PIN_DRIVER_PUL, LOW);
    delayMicroseconds(MOTOR_PULSE_DELAY - 20);

    if (MOTOR_DIRECTION == HIGH)
    {
        CURRENT_STEP_POSITION--;
    }
    else
    {
        CURRENT_STEP_POSITION++;
    }

    //Serial.print("CURRENT_STEP_POSITION: ");
    //Serial.println(CURRENT_STEP_POSITION);
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
    DisplayMessage(0, 30, String(CURRENT_STEP_POSITION));

    BUTTON_PRESSED = -1;
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
    display.println( String(CURRENT_STEP_POSITION ));

    display.display();
}

/*****************************************************
 * LerpLinear(int from, int to, int deltaSteps)
 * interpolates a values with in a linear manner
 * 
 * from the starting value
 * to: the final value
 * deltaSteps: the number of steps when called in a loop = loop counter
 * 
 * Example: (in a loop)
 * MOTOR_PULSE_DELAY = LerpLinear(startMotorPulseDelay, maxMotorPulseDelay, loopCounter);
 */
unsigned int LerpLinear(unsigned int from, unsigned int to, unsigned int deltaSteps)
{
    if (deltaSteps > ACCEL_STEPS )
    {
        return to;
    }

    int diffValue = to - from;
    float valuePerStep = (float)diffValue / (float)(ACCEL_STEPS);

    return from + ceil((float)deltaSteps * valuePerStep);
}


/*****************************************************
 * LinearMap(long ax, long aMin, long aMax, long bMin, long bMax)
 * Maps value ax from range aMin/aMax into range bMin/bMax
 */
long LinearMap(long ax, long aMin, long aMax, long bMin, long bMax)
{
    if (ax >= aMax) return bMax; 
    double slope = 1.0 * (bMax-bMin) / (aMax-aMin);
    return bMin + round(slope * (ax + aMin));
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
    //Serial.println(message);
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
    DisplayMessage(0, 20, String(CURRENT_STEP_POSITION));
    
    // loop until a button is pressed to store the current positon to
    // or to cancel storing the position
    while (BUTTON_PRESSED == -1)
    {
        // check for button press
        CheckButtons();
        if ( BUTTON_PRESSED >= 0 && BUTTON_PRESSED <= 11 )
        {
            // store position
            EEPROM.put( CalculateEEPROMAddressForButton(BUTTON_PRESSED), CURRENT_STEP_POSITION );

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



void SaveMotorSettings()
{
    Serial.println("SaveMotorSettings");

    // calculate adress of MOTOR_MIN_SPEED_RPM
    // the first 13 values are unsigned longs
    int address = sizeof(unsigned long) * 13;

    // MOTOR_MIN_SPEED_RPM
    EEPROM.put( address, MOTOR_MIN_SPEED_RPM );
    Serial.print("    Saved MOTOR_MIN_SPEED_RPM: ");
    Serial.print(MOTOR_MIN_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_MIN_SPEED_RPM);

    // MOTOR_MAX_SPEED_RPM
    EEPROM.put( address, MOTOR_MAX_SPEED_RPM );
    Serial.print("    Saved MOTOR_MAX_SPEED_RPM: ");
    Serial.print(MOTOR_MAX_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_MAX_SPEED_RPM);

    // MOTOR_CALIBRATION_SPEED_RPM
    EEPROM.put( address, MOTOR_CALIBRATION_SPEED_RPM );
    Serial.print("    Saved MOTOR_CALIBRATION_SPEED_RPM: ");
    Serial.print(MOTOR_CALIBRATION_SPEED_RPM);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_CALIBRATION_SPEED_RPM);

    // ACCEL_STEPS
    EEPROM.put( address, ACCEL_STEPS );
    Serial.print("    Saved ACCEL_STEPS: ");
    Serial.print(ACCEL_STEPS);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(ACCEL_STEPS);

    // MOTOR_PPR
    EEPROM.put( address, MOTOR_PPR );
    Serial.print("    Saved MOTOR_PPR: ");
    Serial.print(MOTOR_PPR);
    Serial.print(" | ");
    Serial.println(address);
    address += sizeof(MOTOR_PPR);
}


void InterruptTimerCallback()
{
  // This is the Encoder's worker routine. It will physically read the hardware
  // and all most of the logic happens here. Recommended interval for this method is 1ms.
  rotaryEncoder.service();
}