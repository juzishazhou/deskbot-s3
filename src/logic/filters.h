#pragma once

#include <stdint.h>

// 这些类型不依赖 Arduino，可在电脑上直接单元测试。

enum class IrDir : uint8_t {
    None = 0,
    Left,
    Right,
    Both
};

struct TouchState {
    bool pressed = false;       // 消抖后的持续按下状态
    bool pressedEdge = false;   // 按下沿，只出现一轮
    bool releasedEdge = false;  // 松开沿，只出现一轮
    bool longEdge = false;      // 长按满 800ms，一次按住只出现一次
};

class TouchFilter {
public:
    void reset(uint32_t now, bool rawPressed);
    TouchState update(uint32_t now, bool rawPressed);

private:
    bool lastRaw_ = false;
    bool stable_ = false;
    bool longSent_ = false;
    uint32_t rawChangedAt_ = 0;
    uint32_t pressedAt_ = 0;
};

struct IrReading {
    IrDir dir = IrDir::None;
    bool windowDone = false;  // 本轮的 5 次多数表决是否刚完成
};

class IrFilter {
public:
    void reset();
    // 返回 true 表示窗口已满，dir 被更新为新的稳定方向。
    bool addSample(bool leftActive, bool rightActive, IrDir &dir);

private:
    uint8_t count_ = 0;
    uint8_t leftCount_ = 0;
    uint8_t rightCount_ = 0;
};

class IrSampler {
public:
    void reset(uint32_t now);
    IrReading update(uint32_t now, bool leftActive, bool rightActive);

private:
    IrFilter filter_;
    IrDir dir_ = IrDir::None;
    uint32_t lastSampleAt_ = 0;
};
