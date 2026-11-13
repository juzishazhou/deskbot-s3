#include <Arduino.h>

#include "config.h"
#include "hw/display.h"
#include "hw/sensors.h"
#include "hw/servos.h"
#include "logic/behavior.h"
#include "logic/filters.h"
#include "logic/motion.h"

namespace {

TouchFilter gTouch;
IrSampler gIr;
Behavior gBehavior;
Motion gMotion;

uint32_t gNextDisplayAt = 0;

void printBanner()
{
    Serial.println();
    Serial.println("=== DeskBot-S3 / ESP32-S3-DevKitC-1 N16R8 ===");
    Serial.printf("Flash %u MB | PSRAM %u KB | CPU %u MHz\n",
                  (unsigned)(ESP.getFlashChipSize() / (1024u * 1024u)),
                  (unsigned)(ESP.getPsramSize() / 1024u),
                  (unsigned)getCpuFrequencyMhz());
    Serial.println("OLED  SDA=GPIO8  SCL=GPIO9");
    Serial.println("Servo yaw=GPIO4  pitch=GPIO5（外部 5V，共地）");
    Serial.println("Touch=GPIO7  IR left=GPIO10  right=GPIO11");
    Serial.println("注意：第一次上电先不装舵盘，回中后再装。");
    Serial.println();
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(300);  // 只等 USB 串口上电，不参与机器人逻辑
    randomSeed(esp_random());

    printBanner();

    sensorsInit();
    servosInit();  // 两轴先回中
    displayInit();

    const uint32_t now = millis();
    gTouch.reset(now, readTouchActive());
    gIr.reset(now);
    gBehavior.reset(now);
    gMotion.reset();

    displaySetMood(Mood::Normal);
    displayTick(now);
    gNextDisplayAt = now + cfg::kDisplayTickMs;
}

void loop()
{
    const uint32_t now = millis();

    // 1. 传感器：硬件层只给出“是否有效”，滤波和表决在纯逻辑层完成。
    const TouchState touch = gTouch.update(now, readTouchActive());
    const IrReading ir =
        gIr.update(now, readIrLeftActive(), readIrRightActive());

    // 2. 行为：触摸 > 红外跟随 > 丢失保持 > 归中。
    BehaviorInput input;
    input.now = now;
    input.ir = ir.dir;
    input.irWindowDone = ir.windowDone;
    input.touch = touch;
    const BehaviorOutput output = gBehavior.update(input);

    // 3. 运动：目标角度按 50°/s 缓动，避免 SG90 瞬间跳转。
    gMotion.command(output.yaw, output.pitch);
    if (gMotion.tick(now)) {
        servosWrite(gMotion.yaw(), gMotion.pitch());
    }

    // 4. 表情：每 50ms 刷一次，内部只在变化或眨眼时真正写屏。
    displaySetMood(output.mood);
    if ((int32_t)(now - gNextDisplayAt) >= 0) {
        displayTick(now);
        gNextDisplayAt = now + cfg::kDisplayTickMs;
    }
}
