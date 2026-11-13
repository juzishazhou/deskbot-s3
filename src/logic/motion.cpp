#include "logic/motion.h"

#include "config.h"

namespace {

int clampAngle(int angle, int minimum, int maximum)
{
    if (angle < minimum) {
        return minimum;
    }
    if (angle > maximum) {
        return maximum;
    }
    return angle;
}

int stepToward(int current, int target)
{
    if (current < target) {
        return current + cfg::kServoStepDegrees;
    }
    if (current > target) {
        return current - cfg::kServoStepDegrees;
    }
    return current;
}

} // namespace

void Motion::reset()
{
    yaw_ = cfg::kYawCenter;
    pitch_ = cfg::kPitchCenter;
    yawTarget_ = cfg::kYawCenter;
    pitchTarget_ = cfg::kPitchCenter;
    lastStepAt_ = 0;
}

void Motion::command(YawAction yaw, PitchAction pitch)
{
    switch (yaw) {
        case YawAction::Hold:
            yawTarget_ = yaw_;
            break;
        case YawAction::Left:
            yawTarget_ = cfg::kYawLeft;
            break;
        case YawAction::Right:
            yawTarget_ = cfg::kYawRight;
            break;
        case YawAction::Center:
            yawTarget_ = cfg::kYawCenter;
            break;
    }

    switch (pitch) {
        case PitchAction::Hold:
            pitchTarget_ = pitch_;
            break;
        case PitchAction::Up:
            pitchTarget_ = cfg::kPitchUp;
            break;
        case PitchAction::Center:
            pitchTarget_ = cfg::kPitchCenter;
            break;
    }

    yawTarget_ = clampAngle(yawTarget_, cfg::kYawMinimum, cfg::kYawMaximum);
    pitchTarget_ = clampAngle(pitchTarget_, cfg::kPitchMinimum, cfg::kPitchMaximum);
}

bool Motion::tick(uint32_t now)
{
    if (now - lastStepAt_ < cfg::kServoStepMs) {
        return false;
    }
    lastStepAt_ = now;

    const int oldYaw = yaw_;
    const int oldPitch = pitch_;
    yaw_ = stepToward(yaw_, yawTarget_);
    pitch_ = stepToward(pitch_, pitchTarget_);
    return yaw_ != oldYaw || pitch_ != oldPitch;
}
