#include "hw/display.h"

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"

namespace {

constexpr int kWidth = 128;
constexpr int kHeight = 64;
constexpr int kLeftEyeX = 40;
constexpr int kRightEyeX = 88;
constexpr int kEyeY = 27;

Adafruit_SSD1306 gDisplay(kWidth, kHeight, &Wire, -1);

bool gReady = false;
bool gWireStarted = false;
bool gDirty = true;
Mood gMood = Mood::Normal;
bool gBlinking = false;
uint32_t gBlinkEndAt = 0;
uint32_t gNextBlinkAt = 0;
uint32_t gLastRetryAt = 0;

bool i2cPresent(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool startDisplay()
{
    if (!gWireStarted) {
        Wire.setPins(cfg::kPinOledSda, cfg::kPinOledScl);
        Wire.begin();
        Wire.setClock(400000);  // 线长不稳时改成 100000
        gWireStarted = true;
    }

    uint8_t address = 0;
    if (i2cPresent(0x3C)) {
        address = 0x3C;
    } else if (i2cPresent(0x3D)) {
        address = 0x3D;
    } else {
        return false;
    }

    // 最后两个 false：不操作复位脚，也不要让库再次调用 Wire.begin()
    if (!gDisplay.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        return false;
    }

    gDisplay.clearDisplay();
    gDisplay.display();
    gReady = true;
    gDirty = true;
    gNextBlinkAt = millis() + random(cfg::kBlinkIntervalMinMs, cfg::kBlinkIntervalMaxMs);
    Serial.printf("[OLED] 就绪，地址 0x%02X\n", address);
    return true;
}

// ---------------------------- 绘图小工具 ----------------------------

void drawRoundEye(int centerX, int centerY, int radius)
{
    gDisplay.fillCircle(centerX, centerY, radius, SSD1306_WHITE);
    gDisplay.fillCircle(centerX, centerY, 3, SSD1306_BLACK);
    gDisplay.fillCircle(centerX - 3, centerY - 3, 2, SSD1306_WHITE);
}

void drawCheerfulEye(int centerX, int centerY)
{
    gDisplay.drawLine(centerX - 10, centerY + 5, centerX, centerY - 5,
                      SSD1306_WHITE);
    gDisplay.drawLine(centerX, centerY - 5, centerX + 10, centerY + 5,
                      SSD1306_WHITE);
}

void drawClosedEye(int centerX, int centerY)
{
    gDisplay.drawLine(centerX - 10, centerY, centerX + 10, centerY,
                      SSD1306_WHITE);
    gDisplay.drawLine(centerX - 10, centerY + 1, centerX + 10, centerY + 1,
                      SSD1306_WHITE);
}

void drawSmile(int topY)
{
    gDisplay.drawLine(44, topY, 52, topY + 6, SSD1306_WHITE);
    gDisplay.drawLine(52, topY + 6, 64, topY + 8, SSD1306_WHITE);
    gDisplay.drawLine(64, topY + 8, 76, topY + 6, SSD1306_WHITE);
    gDisplay.drawLine(76, topY + 6, 84, topY, SSD1306_WHITE);
}

void drawFlatMouth(int y)
{
    gDisplay.drawRoundRect(52, y, 24, 5, 2, SSD1306_WHITE);
}

void drawWavyMouth(int y)
{
    gDisplay.drawLine(46, y + 2, 52, y, SSD1306_WHITE);
    gDisplay.drawLine(52, y, 58, y + 4, SSD1306_WHITE);
    gDisplay.drawLine(58, y + 4, 64, y, SSD1306_WHITE);
    gDisplay.drawLine(64, y, 70, y + 4, SSD1306_WHITE);
    gDisplay.drawLine(70, y + 4, 76, y, SSD1306_WHITE);
    gDisplay.drawLine(76, y, 82, y + 2, SSD1306_WHITE);
}

void drawHeart(int centerX, int centerY, int size)
{
    const int lobe = size / 2;
    gDisplay.fillCircle(centerX - lobe / 2, centerY - lobe / 2, lobe,
                        SSD1306_WHITE);
    gDisplay.fillCircle(centerX + lobe / 2, centerY - lobe / 2, lobe,
                        SSD1306_WHITE);
    gDisplay.fillTriangle(centerX - size, centerY - lobe / 3,
                          centerX + size, centerY - lobe / 3,
                          centerX, centerY + size, SSD1306_WHITE);
}

void drawFace(Mood mood, bool blinking)
{
    gDisplay.clearDisplay();

    if (blinking && mood != Mood::Heart) {
        drawClosedEye(kLeftEyeX, kEyeY);
        drawClosedEye(kRightEyeX, kEyeY);
        switch (mood) {
            case Mood::Happy:
                drawSmile(40);
                break;
            case Mood::Confused:
                drawWavyMouth(46);
                break;
            default:
                drawFlatMouth(48);
                break;
        }
        gDisplay.display();
        return;
    }

    switch (mood) {
        case Mood::Happy:
            drawCheerfulEye(kLeftEyeX, kEyeY);
            drawCheerfulEye(kRightEyeX, kEyeY);
            drawSmile(40);
            break;

        case Mood::Confused:
            drawRoundEye(kLeftEyeX, kEyeY, 8);
            drawRoundEye(kRightEyeX + 2, kEyeY + 3, 5);
            gDisplay.drawLine(78, 14, 100, 18, SSD1306_WHITE);  // 挑起的眉毛
            drawWavyMouth(46);
            break;

        case Mood::Heart:
            drawHeart(kLeftEyeX, kEyeY, 9);
            drawHeart(kRightEyeX, kEyeY, 9);
            drawSmile(42);
            break;

        case Mood::Normal:
        default:
            drawRoundEye(kLeftEyeX, kEyeY, 9);
            drawRoundEye(kRightEyeX, kEyeY, 9);
            drawFlatMouth(48);
            break;
    }

    gDisplay.display();
}

} // namespace

void displayInit()
{
    gLastRetryAt = millis();
    if (!startDisplay()) {
        Serial.println("[OLED] 未找到 0x3C/0x3D，检查 SDA=GPIO8、SCL=GPIO9、VCC=3V3");
        Serial.println("[OLED] 会每 2 秒重试，舵机和传感器仍可正常工作");
    }
}

bool displayReady()
{
    return gReady;
}

void displaySetMood(Mood mood)
{
    if (mood != gMood) {
        gMood = mood;
        gDirty = true;
    }
}

void displayTick(uint32_t now)
{
    if (!gReady) {
        if (now - gLastRetryAt >= 2000) {
            gLastRetryAt = now;
            startDisplay();
        }
        if (!gReady) {
            return;
        }
    }

    if (!gBlinking && gMood != Mood::Heart &&
        (int32_t)(now - gNextBlinkAt) >= 0) {
        gBlinking = true;
        gBlinkEndAt = now + cfg::kBlinkClosedMs;
        gDirty = true;
        gNextBlinkAt = now + random(cfg::kBlinkIntervalMinMs, cfg::kBlinkIntervalMaxMs);
    }

    if (gBlinking && (int32_t)(now - gBlinkEndAt) >= 0) {
        gBlinking = false;
        gDirty = true;
    }

    if (gDirty) {
        drawFace(gMood, gBlinking);
        gDirty = false;
    }
}
