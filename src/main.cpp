#include <Arduino.h>

#include "config.h"
#include "hw/action_player.h"
#include "hw/display.h"
#include "hw/sensors.h"
#include "hw/servos.h"
#include "hw/webui.h"
#include "logic/behavior.h"
#include "logic/filters.h"
#include "logic/motion.h"

namespace {

TouchFilter gTouch;
IrSampler gIr;
Behavior gBehavior;
Motion gMotion;

uint32_t gNextDisplayAt = 0;
Mood gLastMood = Mood::Normal;

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
    Serial.println("Wi-Fi AP \"Robot-Z\" → http://192.168.4.1/");
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
    webInit();     // 热点 + 80 端口；即使失败也不影响本体行为

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

    // 3. 运动：网页动作优先接管舵机，结束后把控制权交还行为层。
    if (!actionActive()) {
        RobotAction action;
        if (webTakeAction(action)) {
            actionPlay(action, gMotion.yaw(), gMotion.pitch(), now);
        }
    }
    if (actionActive()) {
        if (touch.pressedEdge) {
            actionAbortToCenter(now);  // 触摸优先，同样不阻塞
        }
        actionTick(now);
        if (!actionActive()) {
            gMotion.reset();  // 序列收在中位，行为层从当前姿态继续
        }
    } else {
        gMotion.command(output.yaw, output.pitch);
        if (gMotion.tick(now)) {
            servosWrite(gMotion.yaw(), gMotion.pitch());
        }
    }

    // 4. 表情：只在行为表情变化时下发，网页手动选的表情不会被下一轮覆盖。
    if (output.mood != gLastMood) {
        gLastMood = output.mood;
        displaySetMood(output.mood);
    }
    if ((int32_t)(now - gNextDisplayAt) >= 0) {
        displayTick(now);
        gNextDisplayAt = now + cfg::kDisplayTickMs;
    }

    // 5. 网页服务：必须放在 loop 末尾，内部不阻塞。
    webTick();
}
