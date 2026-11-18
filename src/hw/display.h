#pragma once

#include <stdint.h>

#include "logic/behavior.h"

// OLED 可显示的表情。
// 前四项与 logic/behavior.h 里的 Mood 一一对应（枚举值与含义不动），
// 后三项是给后续小智语音预留的聆听/说话/休眠，纯逻辑层不感知。
enum class Face : uint8_t {
    Normal,
    Happy,
    Confused,
    Heart,
    Listen,
    Speak,
    Sleep,
};

void displayInit();
bool displayReady();
void displaySetMood(Mood mood);  // 行为层入口，签名保持不变
void displaySetFace(Face face);  // 测试固件可直接指定（含新增三项）
void displayTriggerBlink();      // 网页“眨眼”按钮：立即眨一次
void displayTick(uint32_t now);
