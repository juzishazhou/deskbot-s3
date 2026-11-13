#include "hw/servos.h"

#include <Arduino.h>
#include <Servo.h>

#include "config.h"

namespace {

Servo gYawServo;
Servo gPitchServo;
int gLastYaw = -1;
int gLastPitch = -1;

} // namespace

void servosInit()
{
    // SG90 常用 500~2400us；先让两轴回中，再装舵盘。
    gYawServo.attach(cfg::kPinServoYaw, 500, 2400);
    gPitchServo.attach(cfg::kPinServoPitch, 500, 2400);
    gYawServo.write(cfg::kYawCenter);
    gPitchServo.write(cfg::kPitchCenter);
    gLastYaw = cfg::kYawCenter;
    gLastPitch = cfg::kPitchCenter;
}

void servosWrite(int yawAngle, int pitchAngle)
{
    // 只在角度变化时写 PWM，减少无意义的刷新。
    if (yawAngle != gLastYaw) {
        gYawServo.write(yawAngle);
        gLastYaw = yawAngle;
    }
    if (pitchAngle != gLastPitch) {
        gPitchServo.write(pitchAngle);
        gLastPitch = pitchAngle;
    }
}
