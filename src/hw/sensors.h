#pragma once

// 传感器原始电平读取，只做“物理电平 -> 逻辑有效”的转换。
void sensorsInit();
bool readTouchActive();
bool readIrLeftActive();
bool readIrRightActive();
