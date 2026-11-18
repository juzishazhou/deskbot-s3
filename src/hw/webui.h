#pragma once

#include <stdint.h>

// 网页按钮触发的整机动作。
// 只做“请求排队”，真正的舵机动作由主循环用现有角度/缓动参数非阻塞执行。
enum class RobotAction : uint8_t {
    Nod,
    Shake,
    Reset,
};

void webInit();  // 开 AP + 注册路由 + 启动 80 端口服务
void webTick();  // server.handleClient()，放在 loop() 末尾
bool webTakeAction(RobotAction &out);  // 取走一次待执行动作
