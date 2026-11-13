# DeskBot-S3

基于 ESP32-S3-DevKitC-1（N16R8）的桌面表情机器人：一块 128×64 OLED 做脸，两只 SG90 控制头和俯仰，TTP223 触摸 + 左右红外做互动输入。

本项目参考了开源项目 [jaddele/DeskBot](https://github.com/jaddele/DeskBot) 的**分层思路**（纯逻辑状态机 + 可原生测试 + 硬件适配层），但代码是为 ESP32-S3 重新实现的。DeskBot 仓库未附带许可证，因此这里没有复制它的源文件。

## 相比原版做了什么

- **开发板迁移**：ESP32-WROOM-32 → ESP32-S3-DevKitC-1 N16R8；工程启用 16MB Flash、Octal PSRAM、16MB 分区表。
- **引脚重排**：原版 GPIO12/13、21/22、4/5/18 在 S3 上不再适用；全部改为 S3 的安全引脚，并避开 Flash（26–32）、PSRAM（33–37）和 strapping 脚（0/3/45/46）。
- **触摸长按**：短按疑惑脸、按住 800ms 爱心眼；长按只在一次按住中触发一次。
- **红外多数表决**：2ms 采样一次，5 次里至少 4 次有效才改变方向，降低阳光/反光误触发。
- **触摸后红外重新接管规则**：松手后必须先出现一次完整的“无遮挡”窗口，旧遮挡才不会在 3 秒归中后立刻抢回控制权。
- **舵机缓动**：目标角度按 50°/s（1°/20ms）变化，不会瞬间跳到极限位置。
- **OLED 容错**：0x3C/0x3D 自动识别；没插屏时每 2 秒重试，机器人和串口仍然可用。
- **有测试**：触摸消抖、长按边界、红外表决、行为优先级、归中计时、`millis()` 回绕、舵机限速都能在电脑上跑 Unity 测试。

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

## 编译与烧录

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
face normal|happy|confused|heart,
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
  main.cpp              正式固件：组装行为状态机与主循环
  module_test.cpp       测试固件：串口命令与网页分步验收
  config.h              引脚、电平、角度、时间参数
  logic/                纯 C++，不依赖 Arduino，可在电脑上测试
    filters.h/.cpp      触摸消抖/长按、红外多数表决
    behavior.h/.cpp     优先级状态机
    motion.h/.cpp       舵机目标与缓动
  hw/                   只做硬件适配
    sensors.*           GPIO 电平读取
    servos.*            Servo PWM 输出
    display.*           OLED 初始化与表情动画
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

## 说明

DeskBot 原项目未声明开源许可证。本项目仅借鉴其架构与交互设计，代码为独立实现，并以 MIT 许可证发布（见 `LICENSE`）。
