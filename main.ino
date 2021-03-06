
#include "src/config.h"
#include "src/global.h"
#include "src/gyro/Cgyro.h"
#include "src/altimeter/altitude.h"
#include "src/servo/fins_servo.h"
#include "src/flight_correct/correct.h"
#include "src/buzzer/buzzer.h"
#include "src/led_color/led_color.h"
#include "src/storage/Storage.h"
#include "src/storage/LogSystem.h"
#include "src/configuration/configuration.h"
#include "src/data/data.h"
// #include "src/storage/LogSystem_SD.h"
#include "src/parachute/parachute.h"
#include <Wire.h>
#include "src/bluetooth/bluetooth.h"
#include "src/command/command.h"
#include "src/lib/TaskScheduler.h"

// Configuration& conf = _CONF; //Configuration::instance();

float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
bool is_abort = false;
bool is_parachute_deployed = false;
int16_t g_servo_pitch = 0;
int16_t g_servo_yaw = 0;
int16_t g_servo_roll = 0;
byte IS_READY_TO_FLY;   // Used with the REMOVE_BEFORE_FLIGHT jumper

// const long interval = 100;
unsigned long previousMillis = 0;
unsigned long previousHBeatMillis = 0;
bool setup_error = false;


Altitude altitude;
Gyro gyro;
float voltage = 0;
CliCommand cli;
bool ledStatus;
// Scheduler
Scheduler ts;

// ================================================================
// Tasks definition
// ================================================================
void flashLEDcb();
void beepSequencecb();
void updateBLEdiags_cb();
void updateBLEparams_cb();
void updateBLEGuidanceConfig_cb();
void measureVoltage_cb();
Task tflashLED ( 1 * TASK_SECOND, -1, &flashLEDcb, &ts, true );
Task tbeepSequence ( 2 * TASK_SECOND, -1, &beepSequencecb, &ts, true );
Task tMeasureVoltage ( 10 * TASK_SECOND, -1, &measureVoltage_cb, &ts, true );
Task tupdateBLEdiags ( 200 * TASK_MILLISECOND, -1, &updateBLEdiags_cb, &ts, true );     // BLE Diagnostics (fast refresh)
Task tupdateBLEparams ( 500 * TASK_MILLISECOND, -1, &updateBLEparams_cb, &ts, true );   // BLE Params (slow refresh)
Task tupdateBLEGuidanceConfig ( 550 * TASK_MILLISECOND, -1, &updateBLEGuidanceConfig_cb, &ts, true );   // BLE guidance Params (slow refresh)

// ================================================================
// ===               INTERRUPT DETECTION ROUTINE                ===
// ================================================================
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define MPU_INTERRUPT_PIN 35  // use pin 3 as pin 2 interfere with servo.

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void IRAM_ATTR dmpDataReady() {
    portENTER_CRITICAL_ISR(&mux);       // Not sure if this mutex stuff brings anithing...
    mpuInterrupt = true;
    portEXIT_CRITICAL_ISR(&mux);        // Not sure if this mutex stuff brings anithing...
}
 

/*****************************************************************
 *  Used to display sensor data onthe cosole. Mainly to debug.
 *  Should no be used durring flight as it disturbs the interupts of the I2C
 * ***************************************************************/
void displaySensorData() {
    
        // Debug stuff
        // Serial.print(F("Pitch:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.PITCH_AXIS] * 180/M_PI));
        // Serial.print(F("\tYaw:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.YAW_AXIS] * 180/M_PI));
        // Serial.print(F("\tRoll:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.ROLL_AXIS] * 180/M_PI));
        
        // Serial.print(F("\tAltitude:"));
        // Serial.print(altitude.current_altitude);

        // Serial.print(F("\tTemperature:"));
        // Serial.println(altitude.temperature);

        // //***************************************************************************************
        // // Plotter stuff
        // //***************************************************************************************
        // //Serial.print(F("Pitch:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.PITCH_AXIS])); Serial.print(",");
        // //Serial.print(F("\tYaw:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.YAW_AXIS])); Serial.print(",");
        // //Serial.print(F("\tRoll:"));
        // Serial.print((int16_t)(gyro.ypr[_CONF.ROLL_AXIS] )); Serial.print(",");
        // //Serial.print(F("\tAltitude:"));
        // Serial.print(altitude.current_altitude); Serial.print(",");
        // //Serial.print(F("\tTemperature:"));
        // Serial.println(altitude.temperature); 
        // //***************************************************************************************

        // /* Voltage mesurement */
        // float temp;
        // voltage = (float)analogRead(A3);// / 4096 * 38.846;
        // // Serial.println(voltage);
        // voltage = (float)(voltage / 4096 * 40.125);
        // temp = (int8_t)(voltage * 10 + .5);
        // Serial.println((float)(temp/10));
} 


void testSequence() {

    Serial.println(F("Begin of tests..........."));
    Serial.println(F("Testing servos..."));
    testServo();

    Serial.println(F("Testing LED and Buzzer..."));

    // Test LED 
    led_color(LED_COLOR_RED);
    delay(500);               
    led_color(LED_COLOR_GREEN);
    delay(500);               
    led_color(LED_COLOR_BLUE);
    delay(500);               
    led_color(LED_COLOR_OFF);

    // Test PIEZO_BUZZER 
    const int8_t tone_delay = 83;  //1000/12
    buzz(PIEZO_BUZZER, 2637, tone_delay);
    buzz(PIEZO_BUZZER, 3136, tone_delay);
    buzz(PIEZO_BUZZER, 2093, tone_delay);
    buzz(PIEZO_BUZZER, 0, tone_delay);
    Serial.println(F("End of tests..........."));
}

void debugParachute() {

    // Disable the test if the REMOVE_BEFORE_FLY jumper is not present 
    if (IS_READY_TO_FLY == LOW)
        return;

    int16_t countdown = 10;
    // Serial.println("Debug mode. Press any key to deploy parachute....................");
    // while(Serial.available() == 0) { }  // There really is nothing between the {} braces
    // char x = Serial.read();

    while(countdown >=0) {
        delay(1000);
        buzz(PIEZO_BUZZER, 2637, 1000/12);
        countdown--;
    }

    buzz(PIEZO_BUZZER, 2637, 1000/12);
    buzz(PIEZO_BUZZER, 2637, 1000/12);
    buzz(PIEZO_BUZZER, 2637, 1000/12);
    buzz(PIEZO_BUZZER, 2637, 1000/12);
    buzz(PIEZO_BUZZER, 2637, 10000/12);
    deployParachute();
    is_abort = true;
    is_parachute_deployed = true;
    return;
}

int8_t persistData() {

    if(_CONF.MEMORY_CARD_ENABLED == 0 || IS_READY_TO_FLY == LOW) {
        return 0;
    }

    if(gyro.ypr[1] == 0 || gyro.ypr[2] ==0) {
        // Data invalid do nothing
        return 1;
    }

    lr::LogRecord logRecord(
    // LogRecord logRecord(
        millis(), 
        altitude.current_altitude, 
        (int16_t) (gyro.ypr[_CONF.PITCH_AXIS] * 180/M_PI),  // Pitch: Must be improved
        (int16_t) (gyro.ypr[_CONF.ROLL_AXIS] * 180/M_PI),  // Roll:  Must be improved
        g_servo_pitch, // Servo Pitch: ToDo
        g_servo_roll, // Servo Roll : ToDo
        is_parachute_deployed, 
        is_abort, 
        altitude.temperature, // Temperature
        voltage, // Batt (a faire)
        gyro.z_gforce  // gForces
    );
    if (!lr::LogSystem::appendRecord(logRecord)) {
        Serial.println("Probleme de storrage: verifier memoire pleine");
        return 0;
    } else {
        //Serial.println("Record saved: ");
    }
    return 1;
}


// ================================================================
// ===               MAIN SETUP ROUTINE                         ===
// ================================================================
void setup() {

    is_abort = false;
    is_parachute_deployed = false;
    g_servo_pitch = 0;
    g_servo_roll = 0;

    // initialize serial communication
    Serial.begin(115200);       // Reduced the speed as it was crashing the arduino at 115200
    delay(500);                 // waits for the serial to settle

    _CONF.readConfig();         // Read the config from SPIFFS Memory
    
    pinMode(R_LED, OUTPUT);     digitalWrite(R_LED, HIGH);
    pinMode(G_LED, OUTPUT);     digitalWrite(G_LED, HIGH);
    pinMode(B_LED, OUTPUT);     digitalWrite(B_LED, HIGH);
    led_color(LED_COLOR_BLUE);    // Set the LED to blue durring setup

    pinMode(PIEZO_BUZZER, OUTPUT);
    pinMode(PARACHUTE_IGNITER_PIN, OUTPUT); digitalWrite(PARACHUTE_IGNITER_PIN, LOW);

    //************************************************************************************
    // RESET MEMORY VARIABLES  (Now set from bluetooth)
    //************************************************************************************
    _CONF.FORMAT_MEMORY = 0;
    _CONF.MEMORY_CARD_ENABLED = 0;
    _CONF.DATA_RECOVERY_MODE = 0;

    pinMode(REMOVE_BEFORE_FLIGHT, INPUT_PULLUP); // HIGH IF READY TO FLY. Used with Jumper (Pin is configured as INPUT_PULLUP  ) 
    IS_READY_TO_FLY = digitalRead(REMOVE_BEFORE_FLIGHT); 
    Serial.println(F("=============================================="));
    Serial.print(F("IS_READY_TO_FLY: ")); Serial.println(IS_READY_TO_FLY);
    Serial.println(F("=============================================="));

    // Attach the interrupt pin
    pinMode(MPU_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(MPU_INTERRUPT_PIN), dmpDataReady, RISING);
    
    ledStatus = LOW;

    // Setup bluetooth
    setupBLE(cli);

    setupServo();
    
    if (gyro.setupGyro() != 0) {
        setup_error = true;
        // LED RED
        led_color(LED_COLOR_RED);
        is_abort = true;
        Serial.println(F("Problem with Gyroscope not detected..."));
        return;
    }

    if (altitude.setupAlti() !=0) {
        setup_error = true;
        // LED RED
        led_color(LED_COLOR_RED);
        is_abort = true;
        Serial.println(F("Problem with altitmeter not detected..."));
        return;
    }

    // ================================================================
    // ===               Storage system initialization              ===
    // ================================================================
    Serial.println(F("Initialize the log system"));
    Serial.print("_CONF.MEMORY_CARD_ENABLED : "); Serial.println(_CONF.MEMORY_CARD_ENABLED);
    
    delay(2000);
    // FRAM LOG SYSTEM
    if (!lr::Storage::begin()) {
        Serial.println("Storage Problem");
        is_abort = true;
        return;
    } else {
        lr::LogSystem::begin(0);  
        Serial.println("Storage seems OK");
    }
    // End of Storage system initialization
    // ================================================================    

    testSequence();
    
    // if(_CONF.DEBUG && IS_READY_TO_FLY) {
    //     debugParachute();  // WARNING TEST ONLY! REMOVE THIS LINE BEFORE FLIGHT.
    // }
}

void heartBeat() {
    unsigned long currentHBeatMillis = millis();

    if (setup_error || is_abort) {
        // LED RED
        digitalWrite(R_LED, LOW);
        digitalWrite(G_LED, HIGH);
        digitalWrite(B_LED, HIGH);
        return;
    }

    // Execute all the tasks according to their respective schedules 
    ts.execute();
}



// ================================================================
// ===               Main loop                                  ===
// ================================================================
void loop() {

    unsigned long currentMillis = millis();

    // The abort sequence was triggered (throw your arms in the air) exit the main loop
    // if (is_abort) {
    //     SerialBT.println("is_abort == true");
    //     return;
    // }

    // Some data from the Gyroscope is ready to be processed
    if(mpuInterrupt) {
        portENTER_CRITICAL(&mux);       
        mpuInterrupt = false;
        portEXIT_CRITICAL(&mux);

        gyro.ProcessGyroData();
    }
   
    // Used to slowdown the process to the number of milliseconds defined in variable interval (see config.h file)
     if (currentMillis - previousMillis >= _CONF.SCAN_TIME_INTERVAL) {

        previousMillis = currentMillis; 

        // Read the altitude and process trajectory with the servos
        altitude.processAltiData();
        processTrajectory(gyro.ypr);

        // Debug stuff
        if (_CONF.DEBUG) 
           displaySensorData();  // Output sensors data to serial console.  Enabled only in DEBUG Mode to maximize computer performances.
    
        // Persist flight data to memory
        if (_CONF.DATA_RECOVERY_MODE) {
            _CONF.DATA_RECOVERY_MODE = 0;
            // readDataToSerial();
            readDataToBLE();
        }
        if (_CONF.MEMORY_CARD_ENABLED) {
            persistData();
        }
        if(_CONF.FORMAT_MEMORY) {
                _CONF.FORMAT_MEMORY = 0;
                _CONF.MEMORY_CARD_ENABLED = 0;
                Serial.println(F("**** Erassing memory....This takes a while...."));
                lr::LogSystem::format();
                delay(20000); // 20 Sec. seems to do the job just fine
                Serial.print("Is flash chip busy? : "); Serial.println(lr::LogSystem::isBusy());
            }

        
        heartBeat();

        cli.handleSerial();
    }
   }


/****************************************************************************************************************************
 *                                      TASK CALLBACKs
 ****************************************************************************************************************************/
void flashLEDcb() {

    // Called by task tflashLED
    // Flash Green LED every seconds
    if(ledStatus == LOW) {
        ledStatus = HIGH;
        led_color(LED_COLOR_OFF);
    } else {
        ledStatus = LOW;
        if(!_CONF.MEMORY_CARD_ENABLED) {  
            // If the Memory card is not recording, flash blue
            led_color(LED_COLOR_BLUE);
            delay(50);
            led_color(LED_COLOR_OFF);
            delay(100);
            led_color(LED_COLOR_BLUE);
            delay(50);
            led_color(LED_COLOR_OFF);
            delay(100);
            led_color(LED_COLOR_BLUE);
        }else {
            led_color(LED_COLOR_GREEN);
            delay(50);
            led_color(LED_COLOR_OFF);
            delay(100);
            led_color(LED_COLOR_GREEN);
            delay(50);
            led_color(LED_COLOR_OFF);
            delay(100);
            led_color(LED_COLOR_GREEN);
        }
    }
}

void beepSequencecb() {
    // Called by task tbeepSequence
    if(_CONF.BUZZER_ENABLE) {
        buzz(PIEZO_BUZZER, 2637, 1000/12);
        buzz(PIEZO_BUZZER, 3136, 1000/12);
        buzz(PIEZO_BUZZER, 2093, 1000/12);
    }   
}

void updateBLEdiags_cb() {  // Updated on a fast schedule
    // Called by task updateBLEdiags_cb

    if(!_CONF.DATA_RECOVERY_MODE) {
        updateDiagnostics(gyro.ypr, gyro.aaWorld.x, gyro.aaWorld.y, gyro.aaWorld.z, altitude.current_altitude, altitude.temperature, 
                altitude.pressure, altitude.humidity, voltage);
    }
}
void updateBLEparams_cb() {  // Updated on a slower schedule
    // Called by task updateBLEparas_cb
    if(!_CONF.DATA_RECOVERY_MODE) {
        updateBLEparams();
    }
}
void updateBLEGuidanceConfig_cb() {  // Updated on a slower schedule
    // Called by task tupdateBLEGuidanceConfig
    if(!_CONF.DATA_RECOVERY_MODE) {
        updateGuiding();
    }
}

void measureVoltage_cb() {
        /* Voltage mesurement */
        float temp;
        voltage = (float)analogRead(VOLTAGE);// / 4096 * 38.846;
        // Serial.println(voltage);
        voltage = (float)(voltage / 4096 * 44.5);

        // Debug
        temp = (int8_t)(voltage * 10 + .5);
        //Serial.print("Voltage: "); Serial.println((float)(temp/10));
}
