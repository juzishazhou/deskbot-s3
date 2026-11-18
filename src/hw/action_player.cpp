#include "hw/action_player.h"

#include "config.h"
#include "hw/servos.h"

namespace {

struct PoseStep {
    int16_t yaw;
    int16_t pitch;
    uint16_t holdMs;  // 到位后停留时间
};

// 点头：低头 → 抬头 → 回中（本项目“角度变小=抬头”）
constexpr PoseStep kNodSteps[] = {
    {cfg::kYawCenter, cfg::kPitchDown, 120},
    {cfg::kYawCenter, cfg::kPitchUp, 120},
    {cfg::kYawCenter, cfg::kPitchCenter, 0},
};

// 摇头：左 → 右 → 回中
constexpr PoseStep kShakeSteps[] = {
    {cfg::kYawLeft, cfg::kPitchCenter, 120},
    {cfg::kYawRight, cfg::kPitchCenter, 120},
    {cfg::kYawCenter, cfg::kPitchCenter, 0},
};

constexpr PoseStep kCenterSteps[] = {
    {cfg::kYawCenter, cfg::kPitchCenter, 0},
};

const PoseStep *gSteps = kCenterSteps;
uint8_t gStepCount = 1;
uint8_t gStepIndex = 0;
bool gActive = false;
int16_t gYaw = cfg::kYawCenter;
int16_t gPitch = cfg::kPitchCenter;
uint32_t gStepAt = 0;
uint32_t gHoldUntil = 0;

} // namespace

void actionPlay(RobotAction action, int yawNow, int pitchNow, uint32_t now)
{
    switch (action) {
        case RobotAction::Nod:
            gSteps = kNodSteps;
            gStepCount = sizeof(kNodSteps) / sizeof(kNodSteps[0]);
            break;
        case RobotAction::Shake:
            gSteps = kShakeSteps;
            gStepCount = sizeof(kShakeSteps) / sizeof(kShakeSteps[0]);
            break;
        case RobotAction::Reset:
        default:
            gSteps = kCenterSteps;
            gStepCount = 1;
            break;
    }
    gYaw = static_cast<int16_t>(yawNow);
    gPitch = static_cast<int16_t>(pitchNow);
    gStepIndex = 0;
    gActive = true;
    gStepAt = now;
    gHoldUntil = now;
}

void actionAbortToCenter(uint32_t now)
{
    gSteps = kCenterSteps;
    gStepCount = 1;
    gStepIndex = 0;
    gHoldUntil = now;
}

bool actionActive()
{
    return gActive;
}

void actionTick(uint32_t now)
{
    if (!gActive) {
        return;
    }
    if ((int32_t)(now - gStepAt) < (int32_t)cfg::kServoStepMs) {
        return;
    }
    gStepAt = now;

    const PoseStep &step = gSteps[gStepIndex];
    bool moved = false;
    if (gYaw != step.yaw) {
        gYaw += (gYaw < step.yaw) ? cfg::kServoStepDegrees
                                  : -cfg::kServoStepDegrees;
        moved = true;
    }
    if (gPitch != step.pitch) {
        gPitch += (gPitch < step.pitch) ? cfg::kServoStepDegrees
                                        : -cfg::kServoStepDegrees;
        moved = true;
    }
    if (moved) {
        servosWrite(gYaw, gPitch);
        gHoldUntil = now + step.holdMs;
        return;
    }
    if ((int32_t)(now - gHoldUntil) < 0) {
        return;
    }

    ++gStepIndex;
    if (gStepIndex >= gStepCount) {
        gActive = false;  // 已停在中位，主循环会把 Motion 也归位
        return;
    }
    gHoldUntil = now;
}
