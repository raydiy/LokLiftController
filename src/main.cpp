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
int BUTTON_PRESSED                      = -1;       // the array ID of the button that has been pressed last
unsigned long TOTAL_TRACK_STEPS         = 0;        // [EEPROM] the number of steps from one end stop to the the other end stop
unsigned long START_TIME                = 0;        // used for different situations where a START_TIME is needed
unsigned long CURRENT_STEP_POSITION     = 0;        // the current position of the motor in steps
long ENCODER_POSITION                   = 0;        // the current encoder position
long OLD_ENCODER_POSITION               = 0;        // old encoder position (needed for reading encoder changes)
unsigned long TARGET_POSITIONS[12];                 // [EEPROM] holds the 12 stored positions loaded from EEPROM in steps

byte MOTOR_MODE                         = 0;        // different motorModes: 1 continuos, 2 single step
int MOTOR_PPR                           = 200;      // pulses per revolution of the motor, needed to caclulate the motor speed
int MOTOR_PULSE_DELAY                   = 2000;     // the pulse delay we use in the MotorStep() function = stepping speed
boolean MOTOR_DIRECTION                 = LOW;      // LOW = clockwise rotation
int MOTOR_MIN_SPEED_RPM                 = 25;       // [EEPROM] minimum motor speed in rounds per minute
int MOTOR_MAX_SPEED_RPM                 = 420;      // [EEPROM] maximum motor speed in rounds per minute
int MOTOR_CALIBRATION_SPEED_RPM         = 300;      // [EEPROM] this speed is used during calibration in rpm
unsigned int ACCEL_STEPS                = 400;      // [EEPROM] the number of steps for the acceleration phase in a move

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
int LerpCos(int from, int to, unsigned int deltaSteps);
int LerpLinear(int from, int to, unsigned int deltaSteps);
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
    START_TIME = millis();
    while ( millis() < START_TIME + 5000 )
    { 
        unsigned long m = millis();
        if ( m > START_TIME && m < START_TIME + 1000 ) { DisplayMessage(40, 30, "."); }
        else if ( m > START_TIME && m < START_TIME + 2000 ) { DisplayMessage(40, 30, ".."); }
        else if ( m > START_TIME && m < START_TIME + 3000 ) { DisplayMessage(40, 30, "..."); }
        else if ( m > START_TIME && m < START_TIME + 4000 ) { DisplayMessage(40, 30, "...."); }
        else if ( m > START_TIME && m < START_TIME + 5000 ) { DisplayMessage(40, 30, "....."); }
        CheckButtons();

        // if button 12 (red button) is pressed on startup then start calibration
        if (BUTTON_PRESSED == 12)
        {
            MotorCalibrateEndStops();
        }
        // if button 13 (rotary knob) is pressed on startup then start motor setup
        else if (BUTTON_PRESSED == 13)
        {
            MotorSettings();
        }
    }

    // reset last button pressed
    BUTTON_PRESSED = -1;

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
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOOP ///////////////////////////////////////////////////////////////////////////////////////////////////
//
//
void loop()
{
    /* BUTTONS CHECK LOOP */
    CheckButtons();

    ENCODER_POSITION = rotaryEncoder.read();
    if (OLD_ENCODER_POSITION != ENCODER_POSITION)
    {
        OLD_ENCODER_POSITION = ENCODER_POSITION;
        Serial.println(ENCODER_POSITION);
    }

    // if button 1 to 12 is pressed
    // drive motor to position
    if ( BUTTON_PRESSED >= 0 &&  BUTTON_PRESSED <= 11 )
    {
        unsigned long targetPosition = 0;
        EEPROM.get( CalculateEEPROMAddressForButton(BUTTON_PRESSED), targetPosition );
        MotorMoveTo( targetPosition );
        BUTTON_PRESSED = -1;
    }

    // check for button 12 to initiate saving curent position
    else if (BUTTON_PRESSED == 12)
    {
        SavePosition();
    }

    // check for rotay encoder knob switch press
    else if (BUTTON_PRESSED == 13)
    {
        MotorModeSwitch();
        EncoderReset();
        BUTTON_PRESSED = -1;
    }

    // MOTOR LOOP MODES

    // CONTINOUS MOTOR MODE aka DRIVE MODE aka LAUF-MODUS
    // rotary encoder controls the speed
    if (MOTOR_MODE == 0)
    {
        if (ENCODER_POSITION != 0)
        {
            MOTOR_DIRECTION = ENCODER_POSITION > 0 ? LOW : HIGH;
            MOTOR_PULSE_DELAY = RPM2Delay( MOTOR_MIN_SPEED_RPM ) - abs(ENCODER_POSITION * 100);
            
            // cap the delay to MOTOR_MAX_SPEED_RPM
            // the samller the delay the faster the motor steps
            if ( MOTOR_PULSE_DELAY < RPM2Delay( MOTOR_MAX_SPEED_RPM ) ) { MOTOR_PULSE_DELAY = RPM2Delay( MOTOR_MAX_SPEED_RPM ); }
            MotorStep();

            if ( CheckEndStopA() || CheckEndStopB() )
            {
                ENCODER_POSITION = -ENCODER_POSITION;
                OLD_ENCODER_POSITION = -OLD_ENCODER_POSITION;
                rotaryEncoder.write( ENCODER_POSITION );
            }
        }
    }

    // STEP MOTOR MODE aka STEP MODE aka SCHRITT-MODUS
    // each encoder step is a single motor step
    else if (MOTOR_MODE == 1)
    {
        // Single Motor Step Mode
        if (ENCODER_POSITION != 0)
        {
            MOTOR_DIRECTION = ENCODER_POSITION > 0 ? LOW : HIGH;
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
    OLD_ENCODER_POSITION = 0;
    ENCODER_POSITION = 0;
    rotaryEncoder.write(0);
}


/*****************************************************
 * LoadEEPROMData()
 * Loads the data from EEPROM to the appropriate variables
 */
void LoadEEPROMData()
{
    Serial.println("LoadEEPROMData()");

    int address = 0;
    EEPROM.get(address, TOTAL_TRACK_STEPS);
    address += sizeof(TOTAL_TRACK_STEPS);

    Serial.print("TOTAL_TRACK_STEPS: ");
    Serial.println(TOTAL_TRACK_STEPS);

    for (size_t i = 0; i < 12; i++)
    {
        EEPROM.get(address, TARGET_POSITIONS[i]);
        address += sizeof(TARGET_POSITIONS[i]);

        Serial.print("TARGET_POSITIONS[");
        Serial.print(i);
        Serial.print("]: ");
        Serial.println(TARGET_POSITIONS[i]);
    }
}


/*****************************************************
 * MotorChangeDirection()
 * Changes the direction of the motor
 */
void MotorChangeDirection()
{
    Serial.println("MotorChangeDirection()");
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

    delay(1500);

    // back to main loop
    PrepareForMainLoop();
}


/*****************************************************
 *  MotorSettings()
 * // TODO Settings for motor movement speed
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
    int maxMotorPulseDelay      = RPM2Delay( MOTOR_MAX_SPEED_RPM );
    int startMotorPulseDelay    = 15000;    // the starting speed delay, should be quite high to start slow (should this be adjustable? EEPROM candidate?)
    
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

    MOTOR_DIRECTION = HIGH;

    // set motor speed
    int maxMotorPulseDelay      = RPM2Delay( MOTOR_CALIBRATION_SPEED_RPM );
    int startMotorPulseDelay    = 15000;
    unsigned long stepsDone     = 0;

    while( CheckEndStopA() == false )
    {
        //MOTOR_PULSE_DELAY = LerpCos(startMotorPulseDelay, maxMotorPulseDelay, stepsDone);
        MOTOR_PULSE_DELAY = LerpLinear(startMotorPulseDelay, maxMotorPulseDelay, stepsDone);

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
 * LerpCos(int from, int to, int deltaSteps)
 * interpolates a values with a cosinus curve
 * 
 * from the starting value
 * to: the final value
 * deltaSteps: the number of steps when called in a loop = loop counter
 * 
 * Example: (in a loop)
 * MOTOR_PULSE_DELAY = LerpCos(startMotorPulseDelay, maxMotorPulseDelay, loopCounter);
 */
int LerpCos(int from, int to, unsigned int deltaSteps)
{
    if (deltaSteps >= ACCEL_STEPS )
    {
        return to;
    }

    // how many degrees from 0 to 90 correspüonds the currenmt deltaSteps
    int diffSteps = abs(ACCEL_STEPS - deltaSteps);
    double diffDegree = (90.0/(double)ACCEL_STEPS) * (double)diffSteps;

    // convert degree to radians
    double pi = M_PI;
    double radian = diffDegree * pi/180.0;
    double factor = cos(radian);

    int diffValue = to - from;
    int result = from + round((float)diffValue * factor);

    return result;
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
int LerpLinear(int from, int to, unsigned int deltaSteps)
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