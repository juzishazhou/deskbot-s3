// ============================================================================
// native 环境专用入口（ESP32 构建下本文件为空，不参与链接）
//
// pio test -e native 的 Unity 运行器自带 main，能正常链接；
// 但 pio run -e native 只是“能编能链”的自检目标，src/logic 里没有固件入口，
// MinGW 链接时会报 undefined reference to `WinMain'。
// 这里提供一个普通 main 兜底，测试构建（PIO_UNIT_TESTING）时整体不编译，
// 避免和 Unity 的 main 冲突。
// ============================================================================
#if !defined(ARDUINO) && !defined(PIO_UNIT_TESTING)
int main()
{
    return 0;
}
#endif
