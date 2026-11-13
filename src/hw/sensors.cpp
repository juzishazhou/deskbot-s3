#include "hw/sensors.h"

#include <Arduino.h>

#include "config.h"

namespace {

bool readIrPin(uint8_t pin)
{
    const int level = digitalRead(pin);
    return cfg::kIrActiveLow ? (level == LOW) : (level == HIGH);
}

} // namespace

void sensorsInit()
{
    pinMode(cfg::kPinTouch, INPUT);
    pinMode(cfg::kPinIrLeft, INPUT);
    pinMode(cfg::kPinIrRight, INPUT);
}

bool readTouchActive()
{
    const int level = digitalRead(cfg::kPinTouch);
    return cfg::kTouchActiveHigh ? (level == HIGH) : (level == LOW);
}

bool readIrLeftActive()
{
    return readIrPin(cfg::kPinIrLeft);
}

bool readIrRightActive()
{
    return readIrPin(cfg::kPinIrRight);
}
