#pragma once

#include <stdint.h>

#include "hw/webui.h"

// 网页动作播放器：点头 / 摇头 / 复位。
// 复用 cfg 里的目标角度与 1°/20ms 步进，全程非阻塞，所有序列都收在中位。
// 主循环每轮调用 actionTick()；动作结束后由主循环 reset() Motion 交还控制权。

// 从当前舵机角度起步，避免动作开始时跳变
void actionPlay(RobotAction action, int yawNow, int pitchNow, uint32_t now);
void actionAbortToCenter(uint32_t now);            // 中途放弃，平滑回中
void actionTick(uint32_t now);                     // 推进一步，不做等待
bool actionActive();                               // 是否还有动作在执行
