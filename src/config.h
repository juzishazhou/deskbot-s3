#pragma once

#include <stdint.h>

// ============================================================================
// 硬件接线（ESP32-S3-DevKitC-1 N16R8）
//
//   OLED SSD1306   VCC=3V3  GND=GND  SDA=GPIO8  SCL=GPIO9
//   水平 yaw 舵机   信号=GPIO4   电源=外部 5V（与开发板共地）
//   俯仰 pitch 舵机 信号=GPIO5   电源=外部 5V（与开发板共地）
//   TTP223         VCC=3V3  GND=GND  OUT=GPIO7
//   左红外          VCC=3V3  GND=GND  OUT=GPIO10
//   右红外          VCC=3V3  GND=GND  OUT=GPIO11
//
// 注意：GPIO26~32 接 16MB Flash，GPIO33~37 接 Octal PSRAM，
//       都不能当普通 IO 使用；GPIO0/3/45/46 是启动 strapping 脚。
// ============================================================================
namespace cfg {

constexpr uint8_t kPinOledSda = 8;
constexpr uint8_t kPinOledScl = 9;
constexpr uint8_t kPinServoYaw = 4;
constexpr uint8_t kPinServoPitch = 5;
constexpr uint8_t kPinTouch = 7;
constexpr uint8_t kPinIrLeft = 10;
constexpr uint8_t kPinIrRight = 11;

// 模块电平：TTP223 默认高有效；常见红外避障模块低电平表示检测到障碍。
constexpr bool kTouchActiveHigh = true;
constexpr bool kIrActiveLow = true;

// 触摸消抖与长按
constexpr uint32_t kTouchDebounceMs = 80;
constexpr uint32_t kLongPressMs = 800;

// 红外 2ms 采一次，5 次里至少 4 次有效才翻转方向
constexpr uint8_t kIrSampleCount = 5;
constexpr uint8_t kIrActiveNeeded = 4;
constexpr uint32_t kIrSampleMs = 2;

// 舵机角度。若实机方向相反，交换左右 / 上下的两个角度即可。
constexpr int kYawLeft = 50;
constexpr int kYawRight = 130;
constexpr int kYawCenter = 90;
constexpr int kYawMinimum = 45;
constexpr int kYawMaximum = 135;

constexpr int kPitchUp = 60;      // 抬头（本项目按“角度变小=抬头”）
constexpr int kPitchDown = 120;   // 低头
constexpr int kPitchCenter = 90;
constexpr int kPitchMinimum = 55;
constexpr int kPitchMaximum = 125;

// 舵机每 20ms 最多走 1°，即约 50°/s
constexpr uint32_t kServoStepMs = 20;
constexpr int kServoStepDegrees = 1;

// 互动后保持 3 秒再归中
constexpr uint32_t kReturnCenterMs = 3000;

// OLED 动画节拍
constexpr uint32_t kDisplayTickMs = 50;
constexpr uint32_t kBlinkIntervalMinMs = 2800;
constexpr uint32_t kBlinkIntervalMaxMs = 6200;
constexpr uint32_t kBlinkClosedMs = 120;

} // namespace cfg
