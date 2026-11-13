#pragma once

#include <stdint.h>

#include "logic/behavior.h"

// 与硬件无关的舵机角度规划：目标按 50°/s 缓动，避免瞬间跳转。
class Motion {
public:
    void reset();

    // 每轮 loop 都可以安全调用；Hold 会把目标锁在当前角度。
    void command(YawAction yaw, PitchAction pitch);

    // 按 20ms 节拍最多走 1°，返回本轮角度是否变化。
    bool tick(uint32_t now);

    int yaw() const { return yaw_; }
    int pitch() const { return pitch_; }

private:
    int yaw_ = 90;
    int pitch_ = 90;
    int yawTarget_ = 90;
    int pitchTarget_ = 90;
    uint32_t lastStepAt_ = 0;
};
