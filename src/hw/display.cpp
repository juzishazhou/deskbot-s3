#include "hw/display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "face_res.h"

namespace {

// 在线探测周期：沿用改造前无屏时每 2 秒重试的节奏，不提高轮询频率。
constexpr uint32_t kProbeIntervalMs = 2000;

Adafruit_SSD1306 gDisplay(face_res::kFaceWidth, face_res::kFaceHeight, &Wire, -1);

bool gReady = false;        // 在线且已完成初始化
bool gPresentLast = false;  // 上一轮探测到的在线状态
bool gWireStarted = false;
bool gDirty = true;
Face gFace = Face::Normal;
bool gBlinking = false;
uint32_t gBlinkEndAt = 0;
uint32_t gNextBlinkAt = 0;
uint32_t gLastProbeAt = 0;

bool i2cPresent(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void ensureBus()
{
    if (gWireStarted) return;
    Wire.setPins(cfg::kPinOledSda, cfg::kPinOledScl);
    Wire.begin();
    Wire.setClock(400000);  // 线长不稳时改成 100000
    gWireStarted = true;
}

// 地址自动识别：0x3C 优先，失败再试 0x3D；都没有返回 0。
uint8_t probeAddress()
{
    if (i2cPresent(0x3C)) return 0x3C;
    if (i2cPresent(0x3D)) return 0x3D;
    return 0;
}

// SSD1306 完整初始化。
// 热插拔后屏幕常常停在「I²C 能应答、但显示输出已关闭」的状态，
// 只探测地址不会点亮，必须重跑整套初始化命令（库内部含 0xAF 开显示），
// 再清屏 + 显式开显示兜底，最后由 displayTick 按当前表情重绘一帧。
bool fullInit(uint8_t address, bool reconnect)
{
    ensureBus();

    // 最后两个 false：不操作复位脚，也不要让库再次调用 Wire.begin()
    if (!gDisplay.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        return false;
    }
    gDisplay.ssd1306_command(SSD1306_DISPLAYON);
    gDisplay.clearDisplay();
    gDisplay.display();

    gReady = true;
    gDirty = true;
    gBlinking = false;
    gNextBlinkAt =
        millis() + random(cfg::kBlinkIntervalMinMs, cfg::kBlinkIntervalMaxMs);

    if (reconnect) {
        Serial.printf("[OLED] 检测到屏幕接入，自动初始化完成（地址 0x%02X）\n",
                      address);
    } else {
        Serial.printf("[OLED] 就绪，地址 0x%02X\n", address);
    }
    return true;
}

// ---------------------------- 表情位图 ----------------------------

bool hasBlinkFrame(Face face)
{
    return face == Face::Normal || face == Face::Happy || face == Face::Confused;
}

// 位图全部在 face_res.h，横向扫描 128×64，直接整屏贴图。
const unsigned char *bitmapFor(Face face, bool blinking)
{
    switch (face) {
        case Face::Happy:
            return blinking ? face_res::face_happy_blink : face_res::face_happy;
        case Face::Confused:
            return blinking ? face_res::face_confused_blink
                            : face_res::face_confused;
        case Face::Heart:
            return face_res::face_heart;
        case Face::Listen:
            return face_res::face_listen;
        case Face::Speak:
            return face_res::face_speak;
        case Face::Sleep:
            return face_res::face_sleep;
        case Face::Normal:
        default:
            return blinking ? face_res::face_normal_blink : face_res::face_normal;
    }
}

void drawFace(Face face, bool blinking)
{
    gDisplay.clearDisplay();
    gDisplay.drawBitmap(0, 0, bitmapFor(face, blinking), face_res::kFaceWidth,
                        face_res::kFaceHeight, SSD1306_WHITE);
    gDisplay.display();
}

} // namespace

void displayInit()
{
    gLastProbeAt = millis();

    ensureBus();
    const uint8_t address = probeAddress();
    if (address != 0 && fullInit(address, false)) {
        gPresentLast = true;
        return;
    }

    Serial.println("[OLED] 未找到 0x3C/0x3D，检查 SDA=GPIO8、SCL=GPIO9、VCC=3V3");
    Serial.println("[OLED] 会每 2 秒重试，舵机和传感器仍可正常工作");
}

bool displayReady()
{
    return gReady;
}

void displaySetFace(Face face)
{
    if (face != gFace) {
        gFace = face;
        gDirty = true;
    }
}

void displaySetMood(Mood mood)
{
    switch (mood) {
        case Mood::Happy:
            displaySetFace(Face::Happy);
            break;
        case Mood::Confused:
            displaySetFace(Face::Confused);
            break;
        case Mood::Heart:
            displaySetFace(Face::Heart);
            break;
        case Mood::Normal:
        default:
            displaySetFace(Face::Normal);
            break;
    }
}

void displayTriggerBlink()
{
    // 爱心/聆听/说话/休眠没有眨眼帧，退回平静脸保证这次眨眼看得见
    if (!hasBlinkFrame(gFace)) {
        gFace = Face::Normal;
    }
    const uint32_t now = millis();
    gBlinking = true;
    gBlinkEndAt = now + cfg::kBlinkClosedMs;
    gNextBlinkAt =
        now + random(cfg::kBlinkIntervalMinMs, cfg::kBlinkIntervalMaxMs);
    gDirty = true;
}

void displayTick(uint32_t now)
{
    // 1. 在线状态探测：每 2 秒一次，只在「离线 → 在线」跳变时完整重初始化。
    if (now - gLastProbeAt >= kProbeIntervalMs) {
        gLastProbeAt = now;
        const uint8_t address = probeAddress();
        const bool present = address != 0;

        if (present && !gPresentLast) {
            // 刚上线：重初始化失败则保持离线，下轮继续探测
            gPresentLast = fullInit(address, true);
        } else if (!present && gPresentLast) {
            // 刚掉线：不操作显示对象，其他模块照常跑
            gPresentLast = false;
            gReady = false;
            Serial.println("[OLED] 屏幕已断开，其他功能继续运行");
        }
        // 在线保持期间不重复 begin()，避免刷屏、占用总线
    }

    if (!gReady) return;

    // 2. 空闲自动眨眼（爱心/聆听/说话/休眠不眨眼，与改造前爱心脸的规则一致）
    if (!gBlinking && hasBlinkFrame(gFace) && (int32_t)(now - gNextBlinkAt) >= 0) {
        gBlinking = true;
        gBlinkEndAt = now + cfg::kBlinkClosedMs;
        gDirty = true;
        gNextBlinkAt =
            now + random(cfg::kBlinkIntervalMinMs, cfg::kBlinkIntervalMaxMs);
    }

    if (gBlinking && (int32_t)(now - gBlinkEndAt) >= 0) {
        gBlinking = false;
        gDirty = true;
    }

    // 3. 只在表情变化或眨眼时真正写屏
    if (gDirty) {
        drawFace(gFace, gBlinking);
        gDirty = false;
    }
}
