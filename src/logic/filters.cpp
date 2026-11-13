#include "logic/filters.h"

#include "config.h"

// ============================================================================
// 触摸：先做 80ms 消抖，再在按住满 800ms 时发一次长按事件。
// 即使在 800ms 边界上恰好松手，也会在下降沿补发长按事件。
// ============================================================================
void TouchFilter::reset(uint32_t now, bool rawPressed)
{
    lastRaw_ = rawPressed;
    stable_ = false;
    longSent_ = false;
    rawChangedAt_ = now;
    pressedAt_ = now;
}

TouchState TouchFilter::update(uint32_t now, bool rawPressed)
{
    TouchState state;

    if (rawPressed != lastRaw_) {
        // 在原始下降沿检查长按，避免松手消抖把边界事件吃掉。
        if (lastRaw_ && !rawPressed && stable_ && !longSent_ &&
            now - pressedAt_ >= cfg::kLongPressMs) {
            longSent_ = true;
            state.longEdge = true;
        }
        lastRaw_ = rawPressed;
        rawChangedAt_ = now;
    }

    if (rawPressed != stable_ && now - rawChangedAt_ >= cfg::kTouchDebounceMs) {
        stable_ = rawPressed;
        if (stable_) {
            pressedAt_ = now;
            longSent_ = false;
            state.pressedEdge = true;
        } else {
            state.releasedEdge = true;
            longSent_ = false;
        }
    }

    if (stable_ && rawPressed && !longSent_ &&
        now - pressedAt_ >= cfg::kLongPressMs) {
        longSent_ = true;
        state.longEdge = true;
    }

    state.pressed = stable_;
    return state;
}

// ============================================================================
// 红外：滑动窗口多数表决，抑制阳光、反光造成的单次误触发。
// ============================================================================
void IrFilter::reset()
{
    count_ = 0;
    leftCount_ = 0;
    rightCount_ = 0;
}

bool IrFilter::addSample(bool leftActive, bool rightActive, IrDir &dir)
{
    if (leftActive) {
        ++leftCount_;
    }
    if (rightActive) {
        ++rightCount_;
    }
    ++count_;

    if (count_ < cfg::kIrSampleCount) {
        return false;
    }

    const bool left = leftCount_ >= cfg::kIrActiveNeeded;
    const bool right = rightCount_ >= cfg::kIrActiveNeeded;

    if (left && right) {
        dir = IrDir::Both;
    } else if (left) {
        dir = IrDir::Left;
    } else if (right) {
        dir = IrDir::Right;
    } else {
        dir = IrDir::None;
    }

    reset();
    return true;
}

void IrSampler::reset(uint32_t now)
{
    filter_.reset();
    dir_ = IrDir::None;
    lastSampleAt_ = now;
}

IrReading IrSampler::update(uint32_t now, bool leftActive, bool rightActive)
{
    IrReading reading;
    reading.dir = dir_;

    if (now - lastSampleAt_ < cfg::kIrSampleMs) {
        return reading;
    }
    lastSampleAt_ = now;

    reading.windowDone = filter_.addSample(leftActive, rightActive, dir_);
    reading.dir = dir_;
    return reading;
}
