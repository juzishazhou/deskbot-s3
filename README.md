# DeskBot-S3

基于 ESP32-S3-DevKitC-1（N16R8）的桌面表情机器人：一块 128×64 OLED 做脸，两只 SG90 控制水平和俯仰，TTP223 触摸 + 左右红外做互动输入，手机连热点即可远程控制。

## 功能特性

- **硬件平台**：ESP32-S3-DevKitC-1 N16R8，工程启用 16MB Flash、8MB Octal PSRAM 与 16MB 分区表。
- **引脚规划**：全部使用 S3 安全引脚，避开 Flash（26–32）、PSRAM（33–37）和 strapping 脚（0/3/45/46）。
- **触摸长按**：短按疑惑脸、按住 800ms 爱心眼；长按只在一次按住中触发一次。
- **红外多数表决**：2ms 采样一次，5 次里至少 4 次有效才改变方向，降低阳光/反光误触发。
- **触摸后红外重新接管规则**：松手后必须先出现一次完整的“无遮挡”窗口，旧遮挡才不会在 3 秒归中后立刻抢回控制权。
- **舵机缓动**：目标角度按 50°/s（1°/20ms）变化，不会瞬间跳到极限位置。
- **OLED 容错**：0x3C/0x3D 自动识别；没插屏时每 2 秒重试，机器人和串口仍然可用。
- **OLED 热插拔自愈**：运行中每 2 秒探测一次，离线→在线的瞬间重新初始化并重绘当前表情，插拔屏幕不用重启。
- **表情资源独立**：位图从 `display.cpp` 拆到 `src/face_res.h`，统一放在 PROGMEM；共 10 帧（normal/happy/confused/heart 各一张，另加眨眼与 listen/speak/sleep）。
- **Wi-Fi 网页控制**：手机连热点 `Robot-Z`、打开 `http://192.168.4.1/`，就能切表情、点头、摇头、复位。
- **有测试**：触摸消抖、长按边界、红外表决、行为优先级、归中计时、`millis()` 回绕、舵机限速都能在电脑上跑 Unity 测试。

## 快速开始

```bash
pio run -t upload      # 编译并上传正式固件（UART 口）
pio device monitor     # 查看串口日志
```

手机连接热点 `Robot-Z`（密码 `12345678`），浏览器打开 <http://192.168.4.1/> 即可控制表情与动作。

## 硬件

| 器件 | 数量 | 说明 |
|---|---:|---|
| ESP32-S3-DevKitC-1 N16R8 | 1 | 16MB Flash + 8MB Octal PSRAM |
| SSD1306 OLED 128×64 | 1 | I²C，3.3V |
| SG90 舵机 | 2 | 水平 + 俯仰，**外部 5V 供电** |
| TTP223 触摸模块 | 1 | 3.3V，高电平有效 |
| 红外避障模块 | 2 | 3.3V，低电平表示检测到障碍（可配置） |
| 5V/2A 以上电源 | 1 | 与开发板共地；舵机电源端并 470–1000µF 电容 |

### 接线

| 模块 | 模块引脚 | ESP32-S3-DevKitC-1 |
|---|---|---|
| OLED | VCC / GND | 3V3 / GND |
| OLED | SDA / SCL | **GPIO8 / GPIO9** |
| 水平 yaw 舵机 | 信号 | **GPIO4** |
| 俯仰 pitch 舵机 | 信号 | **GPIO5** |
| 舵机 | VCC / GND | 外部 5V / 与开发板共地 |
| TTP223 | VCC / GND / OUT | 3V3 / GND / **GPIO7** |
| 左红外 | VCC / GND / OUT | 3V3 / GND / **GPIO10** |
| 右红外 | VCC / GND / OUT | 3V3 / GND / **GPIO11** |

> 烧录时使用开发板上标 **UART** 的 USB 口（不是标 USB 的 OTG 口）。
> 第一次上电**不要装舵盘**：程序会让两轴先回中，再按 90° 位置装摇臂。

## 互动规则

1. 左/右红外稳定触发：向对应方向转头、抬头、开心眼。
2. 两侧同时触发：水平方向保持，继续抬头，普通表情（避免来回抢方向）。
3. 红外目标消失：原地保持并显示疑惑，3 秒后平滑归中。
4. 触摸按下：立即锁住水平方向、抬头、疑惑眼。
5. 按住满 800ms 且未松手：切换爱心眼；一次按住只触发一次。
6. 松手后：保持当前表情 3 秒再归中；期间旧遮挡不会立刻重新接管。
7. 松手后必须先经历一次完整的“无遮挡”红外窗口，新的遮挡才允许重新跟随。
8. 空闲时每 2.8–6.2 秒自动眨眼一次。

## 手机网页控制

开发板上电即开热点，不需要路由器，也不需要装 App：

1. 手机 Wi-Fi 连接 **`Robot-Z`**，密码 **`12345678`**；
2. 浏览器打开 **<http://192.168.4.1/>**（AP 固定 IP，串口开机日志也会打印）；
3. 点按钮即可，按钮会有绿色闪烁 + 底部提示作为反馈。

> 这个热点没有外网。部分 Android 机型会提示“网络不可用/是否继续使用”，选择继续即可。

页面只有两个卡片：

| 卡片 | 按钮 | 效果 |
|---|---|---|
| 表情 | 正常 / 开心 / 眨眼 | 平静脸 / 笑眼 / 立即眨一次 |
| 动作 | 点头 / 摇头 / 复位 | 低头抬头一次 / 左右各摆一次 / 两轴回正中 |

### HTTP 接口

均为 `GET`，返回 JSON：成功 `{"ok":true,...}`，参数错误时 `400` + `{"ok":false,"error":"..."}`。

| 路径 | 参数 | 说明 |
|---|---|---|
| `/` | — | 移动端控制页（内嵌 PROGMEM，单文件无外部依赖） |
| `/api/face?type=normal` | `normal` / `happy` / `blink` | 调用 `displaySetMood()` 切表情；`blink` 调 `displayTriggerBlink()` 立即眨眼 |
| `/api/action?do=nod` | `nod` / `shake` / `reset` | 点头 / 摇头 / 复位回中 |

行为说明：

- 网页动作不在 HTTP 回调里执行，只置一个待执行标志；主循环用**目标角度 + 1°/20ms 缓动**走完整个序列，全程没有阻塞式 `delay()`，串口、红外、触摸照常响应。
- 每个序列都收在正中，结束后把舵机状态交还行为层，不会和红外跟随打架。
- 动作执行中按一下触摸会立即中止并平滑回中（触摸优先级最高）。
- 表情是共享的：网页选的表情会一直保持，直到行为层自己切换表情（例如红外触发开心、松手后疑惑）。也就是说“谁最后改谁说话”，不会每轮循环被覆盖。
- 串口命令和触摸/红外交互都没有改动，网页只是多了一个入口。

## 编译与烧录

工程有三个 `env`，用途不同（完整步骤见 [`docs/编译与烧录全流程.md`](docs/编译与烧录全流程.md)）：

| env | 用途 | 命令 |
|---|---|---|
| `esp32-s3-devkitc-1` | 正式固件（默认 env，`pio run` 就是它） | `pio run -e esp32-s3-devkitc-1 -t upload` |
| `module_test` | 分步硬件验收固件（无 Wi-Fi） | `pio run -e module_test -t upload` |
| `native` | 电脑上的单元测试 | `pio test -e native` |

```bash
# 编译
pio run

# 烧录（UART 口）
pio run -t upload

# 查看串口
pio device monitor
```

上传超时时：

1. 按住 BOOT，点一下 RST，松开 BOOT，再执行上传；
2. 把 `platformio.ini` 里的 `upload_speed` 改成 `460800` 或 `115200`。

## 分步硬件测试

第一次组装不要直接烧正式固件。先烧专用测试固件，在 PlatformIO 串口监视器里逐模块验证，再烧正式固件做整体验证：

```bash
# 烧录测试固件
pio run -e module_test -t upload

# 打开串口监视器，输入 help 查看命令
pio device monitor
```

测试固件支持的串口命令：

```text
help, status, stream on|off,
face normal|happy|confused|heart|listen|speak|sleep,
yaw <角度>, pitch <角度>,
center, servotest, stop
```

状态帧格式：`S,OLED,触摸,左红外,右红外,水平角度,俯仰角度,自检中`。

验证顺序为 **OLED → 双红外 → 水平舵机 → 俯仰舵机 → 触摸 → 正式固件**。舵机测试必须空载，使用独立 5V 并与开发板共地。全部通过后烧回正式固件：

```bash
pio run -e esp32-s3-devkitc-1 -t upload
```

完整接线、串口命令、逐模块验收标准、整体行为验收和排障方法见 [`docs/接线与烧录验证指南.md`](docs/接线与烧录验证指南.md)。

## 电脑上的单元测试

不需要开发板，纯逻辑层直接在电脑上跑：

```bash
pio test -e native
```

覆盖内容：

- 触摸 80ms 消抖、短按/长按、800ms 边界松手、时间戳回绕；
- 红外 5 次 4 票多数表决、左右同时触发；
- 行为优先级、红外丢失保持与 3 秒归中、触摸等待、重新接管规则；
- 舵机 1°/20ms 限速、限位、归中保持。

`pio run -e native` 只做“native 环境能编能链”的自检（入口在 `src/native/stub_main.cpp`，跑测试时不会编译它），不涉及硬件。

## 调参

所有硬件与行为参数都在 [`src/config.h`](src/config.h)：

| 参数 | 作用 |
|---|---|
| `kYawLeft` / `kYawRight` | 左右转头角度；方向相反就交换 |
| `kPitchUp` / `kPitchDown` | 抬头/低头角度；方向相反就交换 |
| `kYawMinimum` / `kYawMaximum` | 机械安全限位 |
| `kTouchActiveHigh` | TTP223 实际为低有效时改成 `false` |
| `kIrActiveLow` | 红外模块高有效时改成 `false` |
| `kLongPressMs` | 长按阈值，默认 800ms |
| `kReturnCenterMs` | 互动结束后多久归中，默认 3 秒 |
| `kServoStepMs` / `kServoStepDegrees` | 舵机速度，默认 50°/s |

## 代码结构

```text
src/
  main.cpp              正式固件：传感器采样、行为状态机与主循环装配
  module_test.cpp       测试固件：串口命令分步验收（不含 Wi-Fi）
  config.h              引脚、电平、角度、时间参数
  face_res.h            表情位图（1-bit PROGMEM，16 字节/行）
  logic/                纯 C++，不依赖 Arduino，可在电脑上测试
    filters.h/.cpp      触摸消抖/长按、红外多数表决
    behavior.h/.cpp     优先级状态机
    motion.h/.cpp       舵机目标与缓动
  hw/                   只做硬件适配
    sensors.*           GPIO 电平读取
    servos.*            Servo PWM 输出
    display.*           OLED 初始化、表情动画、热插拔自愈
    webui.*             Wi-Fi 热点、HTTP 路由与内嵌控制页
    action_player.*     网页动作序列（点头/摇头/复位，非阻塞）
  native/
    stub_main.cpp       native 环境的编译自检入口（跑测试时不参与）
test/
  test_logic/           Unity 单元测试
```

## 排障

| 现象 | 检查 |
|---|---|
| 上传超时 | 换 UART 口 / 按住 BOOT 再复位 / 降低 `upload_speed` |
| OLED 一直不亮 | SDA=8、SCL=9 是否接反；VCC 必须 3.3V；串口会打印是否找到 0x3C/0x3D |
| 舵机抖动或复位 | 舵机是否独立 5V 供电、是否共地、是否加了电解电容 |
| 转头/抬头方向相反 | 交换 `config.h` 里对应的两个角度，不要强扭摇臂 |
| 触摸一直触发 | 把 `kTouchActiveHigh` 改成 `false` |
| 红外一直触发 | 把 `kIrActiveLow` 改成 `false`，或检查 3.3V 供电 |
| 手机搜不到热点 | 串口看是否打印 `[WEB] 热点启动失败`；AP 只在 2.4GHz 频段 |
| 页面打不开 | 确认已连 `Robot-Z`、地址是 `http://192.168.4.1/`（不是 https）；关掉手机的移动数据代理再试 |
| 页面按钮无反应 | 动作被执行中的触摸中止属于正常；若一直是“失败”，看串口日志确认服务已启动 |

## 许可证

本项目以 [MIT 许可证](LICENSE) 发布，Copyright (c) 2026 DeskBot-S3 contributors。
