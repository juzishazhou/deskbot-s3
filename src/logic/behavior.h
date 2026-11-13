#pragma once

#include <stdint.h>

#include "logic/filters.h"

enum class YawAction : uint8_t { Hold, Left, Right, Center };
enum class PitchAction : uint8_t { Hold, Up, Center };
enum class Mood : uint8_t { Normal, Happy, Confused, Heart };

struct BehaviorInput {
    uint32_t now = 0;
    IrDir ir = IrDir::None;
    bool irWindowDone = false;
    TouchState touch;
};

struct BehaviorOutput {
    YawAction yaw = YawAction::Hold;
    PitchAction pitch = PitchAction::Hold;
    Mood mood = Mood::Normal;

    BehaviorOutput() = default;
    BehaviorOutput(YawAction y, PitchAction p, Mood m)
        : yaw(y), pitch(p), mood(m) {}
};

// 纯逻辑状态机：不访问 GPIO / Servo / OLED，因此可以在电脑上测试。
// 优先级：触摸 > 红外跟随 > 红外丢失保持 > 归中。
class Behavior {
public:
    void reset(uint32_t now);
    BehaviorOutput update(const BehaviorInput &input);

private:
    enum class Mode : uint8_t {
        Idle,
        Tracking,
        Lost,
        TouchHold,
        TouchWait,
        Returning
    };

    BehaviorOutput follow(IrDir dir) const;

    Mode mode_ = Mode::Idle;
    bool longPress_ = false;
    bool irRearmed_ = true;  // 触摸后必须先看到一次“无遮挡”才允许红外重新接管
    uint32_t lostAt_ = 0;
    uint32_t releasedAt_ = 0;
};
