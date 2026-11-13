#include "logic/behavior.h"

#include "config.h"

BehaviorOutput Behavior::follow(IrDir dir) const
{
    switch (dir) {
        case IrDir::Left:
            return {YawAction::Left, PitchAction::Up, Mood::Happy};
        case IrDir::Right:
            return {YawAction::Right, PitchAction::Up, Mood::Happy};
        case IrDir::Both:
            // 两侧同时有目标时不要来回抢方向，保持水平、继续抬头。
            return {YawAction::Hold, PitchAction::Up, Mood::Normal};
        case IrDir::None:
            break;
    }
    return {YawAction::Center, PitchAction::Center, Mood::Normal};
}

void Behavior::reset(uint32_t now)
{
    (void)now;
    mode_ = Mode::Idle;
    longPress_ = false;
    irRearmed_ = true;
    lostAt_ = 0;
    releasedAt_ = 0;
}

BehaviorOutput Behavior::update(const BehaviorInput &input)
{
    // 触摸按下拥有最高优先级，会打断红外跟随、丢失保持和归中。
    if (input.touch.pressedEdge) {
        mode_ = Mode::TouchHold;
        longPress_ = false;
        irRearmed_ = false;
    }

    if (mode_ == Mode::TouchHold && input.touch.longEdge) {
        longPress_ = true;
    }

    if (mode_ == Mode::TouchHold && input.touch.releasedEdge) {
        mode_ = Mode::TouchWait;
        releasedAt_ = input.now;
        irRearmed_ = false;
    }

    // 只有完整通过多数表决的“无遮挡”窗口，才重新允许红外接管。
    if (input.irWindowDone && input.ir == IrDir::None) {
        irRearmed_ = true;
    }
    const bool canFollow = irRearmed_ && input.ir != IrDir::None;

    switch (mode_) {
        case Mode::TouchHold:
            return {YawAction::Hold, PitchAction::Up,
                    longPress_ ? Mood::Heart : Mood::Confused};

        case Mode::TouchWait:
            if (canFollow) {
                mode_ = Mode::Tracking;
                return follow(input.ir);
            }
            if (input.now - releasedAt_ >= cfg::kReturnCenterMs) {
                mode_ = Mode::Returning;
                return {YawAction::Center, PitchAction::Center, Mood::Normal};
            }
            return {YawAction::Hold, PitchAction::Up,
                    longPress_ ? Mood::Heart : Mood::Confused};

        case Mode::Returning:
            if (canFollow) {
                mode_ = Mode::Tracking;
                return follow(input.ir);
            }
            return {YawAction::Center, PitchAction::Center, Mood::Normal};

        case Mode::Tracking:
            if (canFollow) {
                return follow(input.ir);
            }
            mode_ = Mode::Lost;
            lostAt_ = input.now;
            // 落到 Lost 处理
            // fall through

        case Mode::Lost:
            if (canFollow) {
                mode_ = Mode::Tracking;
                return follow(input.ir);
            }
            if (input.now - lostAt_ >= cfg::kReturnCenterMs) {
                mode_ = Mode::Returning;
                return {YawAction::Center, PitchAction::Center, Mood::Normal};
            }
            return {YawAction::Hold, PitchAction::Hold, Mood::Confused};

        case Mode::Idle:
        default:
            if (canFollow) {
                mode_ = Mode::Tracking;
                return follow(input.ir);
            }
            return {YawAction::Center, PitchAction::Center, Mood::Normal};
    }
}
