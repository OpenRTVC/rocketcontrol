
#include "correct.h"
#include "../global.h"
#include "../parachute/parachute.h"

void processTrajectory(float ypr[3]) {
    int8_t pos_1;
    int8_t pos_2;

    if(ypr[PITCH_AXIS] == 0 || ypr[YAW_AXIS] ==0) {
        // Data invalid do nothing
        return;
    }

    pos_1 =(int8_t) 90-(ypr[PITCH_AXIS] * 180/M_PI);
    pos_2 =(int8_t) 90-(ypr[YAW_AXIS] * 180/M_PI);

    //TODO: Compare angle to EXCESSIVE_ANGLE_THRESHOLD config constant to abort 
    if(pos_1 < (90 - EXCESSIVE_ANGLE_THRESHOLD) || pos_1 > (90 + EXCESSIVE_ANGLE_THRESHOLD) || 
        pos_2 < (90 - EXCESSIVE_ANGLE_THRESHOLD) || pos_2 > (90 + EXCESSIVE_ANGLE_THRESHOLD)) {
        // Here we initiate the abort sequence
        Serial.println("Excessive angle. ABORT SEQUENCE...........");
        Serial.print("pos_1: ");
        Serial.print(pos_1);
        Serial.print("  pos_2: ");
        Serial.print(pos_2);
        Serial.print("  ypr[PITCH_AXIS]: ");
        Serial.print(ypr[PITCH_AXIS]);
        Serial.print("  ypr[YAW_AXIS]: ");
        Serial.println(ypr[YAW_AXIS]);
        deployParachute();
        //is_abort = true;
    } else {
        moveServo(ypr);
    }
}