// ============================================================================
// 模块测试固件（不运行正式行为状态机）
//
// 用途：配合 tools/module-test.html 做分步硬件验收。
//   01 OLED     face normal / happy / confused / heart
//   02 红外     stream on 后观察 S 帧里的 irl/irr
//   03 水平舵机  yaw 85 / yaw 95 / center / servotest
//   04 俯仰舵机  pitch 85 / pitch 95 / center / servotest
//   05 触摸     stream on 后观察 touch 字段
//
// 串口协议（115200，一行一条命令）：
//   help                 打印帮助
//   status               输出一帧状态：S,oled,touch,irl,irr,yaw,pitch,seq
//   stream on|off        每 200ms 自动输出状态帧
//   face <名字>          切换 OLED 表情
//   yaw <角度>           水平舵机目标角度（限位 45~135）
//   pitch <角度>         俯仰舵机目标角度（限位 55~125）
//   center               两轴回 90°
//   servotest            执行一轮 ±5° 小幅自检（先空载、不装舵盘）
//   stop                 停止自检并保持当前位置
// ============================================================================

#include <Arduino.h>

#include "config.h"
#include "hw/display.h"
#include "hw/sensors.h"
#include "hw/servos.h"

namespace {

constexpr uint32_t kStepMs = 20;
constexpr uint32_t kStatusMs = 200;

int gYaw = cfg::kYawCenter;
int gPitch = cfg::kPitchCenter;
int gYawTarget = cfg::kYawCenter;
int gPitchTarget = cfg::kPitchCenter;
uint32_t gLastStepAt = 0;

bool gStream = false;
uint32_t gLastStatusAt = 0;

struct SeqStep {
    int yaw;
    int pitch;
    uint16_t holdMs;
};

// 与教学工具一致的小幅动作：90 → 85 → 95 → 90，两轴依次执行。
const SeqStep kSequence[] = {
    {90, 90, 300},
    {85, 90, 450},
    {95, 90, 450},
    {90, 90, 300},
    {90, 85, 450},
    {90, 95, 450},
    {90, 90, 300},
};

bool gSequenceActive = false;
uint8_t gSequenceIndex = 0;
uint32_t gSequenceStartedAt = 0;

int clampValue(int value, int minimum, int maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

int stepToward(int current, int target)
{
    if (current < target) return current + 1;
    if (current > target) return current - 1;
    return current;
}

void printStatus()
{
    Serial.printf("S,%d,%d,%d,%d,%d,%d,%d\n",
                  displayReady() ? 1 : 0,
                  readTouchActive() ? 1 : 0,
                  readIrLeftActive() ? 1 : 0,
                  readIrRightActive() ? 1 : 0,
                  gYaw, gPitch, gSequenceActive ? 1 : 0);
}

void printHelp()
{
    Serial.println("命令：help | status | stream on|off");
    Serial.println("      face normal|happy|confused|heart");
    Serial.println("      yaw <角度> | pitch <角度> | center");
    Serial.println("      servotest | stop");
}

void applySequenceStep()
{
    const SeqStep &step = kSequence[gSequenceIndex];
    gYawTarget = step.yaw;
    gPitchTarget = step.pitch;
}

void startSequence()
{
    gSequenceActive = true;
    gSequenceIndex = 0;
    gSequenceStartedAt = millis();
    applySequenceStep();
}

void handleCommand(char *line)
{
    // 去掉首尾空白
    while (*line == ' ' || *line == '\t') ++line;
    char *end = line + strlen(line);
    while (end > line && (end[-1] == ' ' || end[-1] == '\t')) --end;
    *end = '\0';
    if (*line == '\0') return;

    for (char *p = line; *p; ++p) {
        if (*p >= 'A' && *p <= 'Z') *p = static_cast<char>(*p - 'A' + 'a');
    }

    char *arg = line;
    while (*arg && *arg != ' ') ++arg;
    if (*arg == ' ') {
        *arg = '\0';
        ++arg;
        while (*arg == ' ') ++arg;
    }

    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        printHelp();
    } else if (strcmp(line, "status") == 0) {
        printStatus();
    } else if (strcmp(line, "stream") == 0) {
        gStream = (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0);
        Serial.printf("OK stream=%d\n", gStream ? 1 : 0);
    } else if (strcmp(line, "face") == 0) {
        if (strcmp(arg, "normal") == 0) {
            displaySetMood(Mood::Normal);
        } else if (strcmp(arg, "happy") == 0) {
            displaySetMood(Mood::Happy);
        } else if (strcmp(arg, "confused") == 0) {
            displaySetMood(Mood::Confused);
        } else if (strcmp(arg, "heart") == 0) {
            displaySetMood(Mood::Heart);
        } else {
            Serial.println("ERR face 只支持 normal/happy/confused/heart");
            return;
        }
        Serial.printf("OK face=%s\n", arg);
    } else if (strcmp(line, "yaw") == 0) {
        gSequenceActive = false;
        gYawTarget = clampValue(atoi(arg), cfg::kYawMinimum, cfg::kYawMaximum);
        Serial.printf("OK yaw=%d\n", gYawTarget);
    } else if (strcmp(line, "pitch") == 0) {
        gSequenceActive = false;
        gPitchTarget = clampValue(atoi(arg), cfg::kPitchMinimum, cfg::kPitchMaximum);
        Serial.printf("OK pitch=%d\n", gPitchTarget);
    } else if (strcmp(line, "center") == 0) {
        gSequenceActive = false;
        gYawTarget = cfg::kYawCenter;
        gPitchTarget = cfg::kPitchCenter;
        Serial.println("OK center");
    } else if (strcmp(line, "servotest") == 0) {
        startSequence();
        Serial.println("OK servotest");
    } else if (strcmp(line, "stop") == 0) {
        gSequenceActive = false;
        gYawTarget = gYaw;
        gPitchTarget = gPitch;
        Serial.println("OK stop");
    } else {
        Serial.println("ERR 未知命令，输入 help");
    }
}

void pollSerial()
{
    static char buffer[64];
    static uint8_t length = 0;

    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            buffer[length] = '\0';
            handleCommand(buffer);
            length = 0;
        } else if (length < sizeof(buffer) - 1) {
            buffer[length++] = c;
        }
    }
}

void updateServos(uint32_t now)
{
    if (now - gLastStepAt < kStepMs) return;
    gLastStepAt = now;

    const int nextYaw = stepToward(gYaw, gYawTarget);
    const int nextPitch = stepToward(gPitch, gPitchTarget);
    if (nextYaw != gYaw || nextPitch != gPitch) {
        gYaw = nextYaw;
        gPitch = nextPitch;
        servosWrite(gYaw, gPitch);
    }
}

void updateSequence(uint32_t now)
{
    if (!gSequenceActive) return;
    if (now - gSequenceStartedAt < kSequence[gSequenceIndex].holdMs) return;

    ++gSequenceIndex;
    if (gSequenceIndex >= sizeof(kSequence) / sizeof(kSequence[0])) {
        gSequenceActive = false;
        gYawTarget = cfg::kYawCenter;
        gPitchTarget = cfg::kPitchCenter;
        Serial.println("OK servotest done");
        return;
    }
    gSequenceStartedAt = now;
    applySequenceStep();
}

} // namespace

void setup()
{
    Serial.begin(115200);
    delay(300);
    randomSeed(esp_random());

    Serial.println();
    Serial.println("DESKBOT-S3-TEST 1.0");
    Serial.println("模块测试固件：OLED SDA=8 SCL=9，舵机 yaw=4 pitch=5");
    Serial.println("触摸=7，左红外=10，右红外=11；舵机外部 5V 并共地");
    Serial.println("警告：servotest 前请空载并取下舵盘。");
    printHelp();

    sensorsInit();
    servosInit();
    displayInit();
    displaySetMood(Mood::Normal);
    displayTick(millis());
    printStatus();
}

void loop()
{
    const uint32_t now = millis();
    pollSerial();
    updateSequence(now);
    updateServos(now);
    displayTick(now);

    if (gStream && now - gLastStatusAt >= kStatusMs) {
        gLastStatusAt = now;
        printStatus();
    }
}
