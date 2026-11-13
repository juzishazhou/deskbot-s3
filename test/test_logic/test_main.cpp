#include <unity.h>

#include "config.h"
#include "logic/behavior.h"
#include "logic/filters.h"
#include "logic/motion.h"

namespace {

TouchState noTouch()
{
    return TouchState{};
}

BehaviorInput makeInput(uint32_t now, IrDir ir, bool windowDone,
                        const TouchState &touch)
{
    BehaviorInput input;
    input.now = now;
    input.ir = ir;
    input.irWindowDone = windowDone;
    input.touch = touch;
    return input;
}

} // namespace

void setUp() {}
void tearDown() {}

// ------------------------------ 触摸滤波 ------------------------------

void test_touch_short_press()
{
    TouchFilter filter;
    filter.reset(0, false);

    TouchState state = filter.update(50, false);
    TEST_ASSERT_FALSE(state.pressedEdge);

    state = filter.update(100, true);  // 原始按下，开始 80ms 消抖
    TEST_ASSERT_FALSE(state.pressedEdge);

    state = filter.update(181, true);
    TEST_ASSERT_TRUE(state.pressedEdge);
    TEST_ASSERT_TRUE(state.pressed);

    state = filter.update(500, false);  // 原始松开
    TEST_ASSERT_FALSE(state.releasedEdge);

    state = filter.update(581, false);
    TEST_ASSERT_TRUE(state.releasedEdge);
    TEST_ASSERT_FALSE(state.pressed);
    TEST_ASSERT_FALSE(state.longEdge);
}

void test_touch_long_press_once()
{
    TouchFilter filter;
    filter.reset(0, true);  // 上电时手已经按在触摸片上

    TouchState state = filter.update(81, true);
    TEST_ASSERT_TRUE(state.pressedEdge);

    state = filter.update(880, true);
    TEST_ASSERT_FALSE(state.longEdge);

    state = filter.update(881, true);
    TEST_ASSERT_TRUE(state.longEdge);

    state = filter.update(950, true);
    TEST_ASSERT_FALSE(state.longEdge);  // 一次按住只报一次

    state = filter.update(1000, false);
    state = filter.update(1081, false);
    TEST_ASSERT_TRUE(state.releasedEdge);
}

void test_touch_long_press_at_release_boundary()
{
    TouchFilter filter;
    filter.reset(0, true);
    filter.update(81, true);

    // 刚好在 800ms 边界松手，下降沿也要补发长按事件
    TouchState state = filter.update(881, false);
    TEST_ASSERT_TRUE(state.longEdge);
    TEST_ASSERT_TRUE(state.pressed);  // 松开仍在消抖窗口内

    state = filter.update(961, false);
    TEST_ASSERT_TRUE(state.releasedEdge);
}

void test_touch_time_wrap()
{
    TouchFilter filter;
    filter.reset(UINT32_MAX - 100u, false);
    filter.update(UINT32_MAX - 50u, true);

    const TouchState state = filter.update(30u, true);
    TEST_ASSERT_TRUE(state.pressedEdge);
}

// ------------------------------ 红外表决 ------------------------------

void test_ir_majority_requires_four_of_five()
{
    IrFilter filter;
    filter.reset();
    IrDir dir = IrDir::None;
    bool complete = false;

    complete = filter.addSample(true, false, dir);
    TEST_ASSERT_FALSE(complete);
    complete = filter.addSample(true, false, dir);
    TEST_ASSERT_FALSE(complete);
    complete = filter.addSample(true, false, dir);
    TEST_ASSERT_FALSE(complete);
    complete = filter.addSample(false, false, dir);
    TEST_ASSERT_FALSE(complete);

    complete = filter.addSample(false, false, dir);
    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IrDir::None),
                          static_cast<int>(dir));  // 只有 3/5，不算有效

    filter.reset();
    filter.addSample(true, false, dir);
    filter.addSample(true, false, dir);
    filter.addSample(true, false, dir);
    filter.addSample(true, false, dir);
    complete = filter.addSample(false, false, dir);
    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IrDir::Left),
                          static_cast<int>(dir));
}

void test_ir_both_sides()
{
    IrFilter filter;
    filter.reset();
    IrDir dir = IrDir::None;
    bool complete = false;
    for (int i = 0; i < cfg::kIrSampleCount; ++i) {
        complete = filter.addSample(true, true, dir);
    }
    TEST_ASSERT_TRUE(complete);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(IrDir::Both),
                          static_cast<int>(dir));
}

// ------------------------------ 行为状态机 ------------------------------

void test_behavior_follows_infrared()
{
    Behavior behavior;
    behavior.reset(0);

    const BehaviorOutput left = behavior.update(
        makeInput(10, IrDir::Left, true, noTouch()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Left),
                          static_cast<int>(left.yaw));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PitchAction::Up),
                          static_cast<int>(left.pitch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Happy),
                          static_cast<int>(left.mood));

    const BehaviorOutput right = behavior.update(
        makeInput(20, IrDir::Right, true, noTouch()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Right),
                          static_cast<int>(right.yaw));
}

void test_behavior_lost_holds_then_centers()
{
    Behavior behavior;
    behavior.reset(0);
    behavior.update(makeInput(10, IrDir::Left, true, noTouch()));

    BehaviorOutput out = behavior.update(
        makeInput(100, IrDir::None, true, noTouch()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PitchAction::Hold),
                          static_cast<int>(out.pitch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Confused),
                          static_cast<int>(out.mood));

    out = behavior.update(makeInput(3099, IrDir::None, false, noTouch()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));

    out = behavior.update(makeInput(3100, IrDir::None, false, noTouch()));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Center),
                          static_cast<int>(out.yaw));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PitchAction::Center),
                          static_cast<int>(out.pitch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Normal),
                          static_cast<int>(out.mood));
}

void test_behavior_touch_priority_and_long_press()
{
    Behavior behavior;
    behavior.reset(0);

    TouchState touch;
    touch.pressed = true;
    touch.pressedEdge = true;

    BehaviorOutput out = behavior.update(
        makeInput(10, IrDir::Left, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(PitchAction::Up),
                          static_cast<int>(out.pitch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Confused),
                          static_cast<int>(out.mood));

    touch.pressedEdge = false;
    touch.longEdge = true;
    out = behavior.update(makeInput(900, IrDir::Left, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Heart),
                          static_cast<int>(out.mood));

    touch.longEdge = false;
    touch.pressed = false;
    touch.releasedEdge = true;
    out = behavior.update(makeInput(1000, IrDir::Left, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(Mood::Heart),
                          static_cast<int>(out.mood));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));
}

void test_behavior_touch_wait_then_center()
{
    Behavior behavior;
    behavior.reset(0);

    TouchState touch;
    touch.pressed = true;
    touch.pressedEdge = true;
    behavior.update(makeInput(10, IrDir::None, true, touch));

    touch.pressedEdge = false;
    touch.pressed = false;
    touch.releasedEdge = true;
    behavior.update(makeInput(100, IrDir::None, true, touch));

    touch.releasedEdge = false;
    BehaviorOutput out = behavior.update(
        makeInput(3099, IrDir::None, false, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));

    out = behavior.update(makeInput(3100, IrDir::None, false, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Center),
                          static_cast<int>(out.yaw));
}

void test_behavior_requires_clear_window_after_touch()
{
    Behavior behavior;
    behavior.reset(0);

    TouchState touch;
    touch.pressed = true;
    touch.pressedEdge = true;
    behavior.update(makeInput(10, IrDir::Left, true, touch));

    touch.pressedEdge = false;
    touch.pressed = false;
    touch.releasedEdge = true;
    behavior.update(makeInput(100, IrDir::Left, true, touch));  // 旧遮挡仍在

    touch.releasedEdge = false;
    BehaviorOutput out = behavior.update(
        makeInput(200, IrDir::Left, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));

    // 先出现一次完整“无遮挡”窗口
    out = behavior.update(makeInput(300, IrDir::None, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Hold),
                          static_cast<int>(out.yaw));

    // 之后的新的遮挡才允许重新接管
    out = behavior.update(makeInput(400, IrDir::Left, true, touch));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(YawAction::Left),
                          static_cast<int>(out.yaw));
}

// ------------------------------ 舵机缓动 ------------------------------

void test_motion_speed_and_limits()
{
    Motion motion;
    motion.reset();
    motion.command(YawAction::Left, PitchAction::Up);

    TEST_ASSERT_FALSE(motion.tick(0));
    TEST_ASSERT_TRUE(motion.tick(cfg::kServoStepMs));
    TEST_ASSERT_EQUAL_INT(89, motion.yaw());
    TEST_ASSERT_EQUAL_INT(89, motion.pitch());

    for (uint32_t now = 40; now <= 4000; now += cfg::kServoStepMs) {
        motion.tick(now);
    }
    TEST_ASSERT_EQUAL_INT(cfg::kYawLeft, motion.yaw());
    TEST_ASSERT_EQUAL_INT(cfg::kPitchUp, motion.pitch());

    motion.command(YawAction::Hold, PitchAction::Hold);
    TEST_ASSERT_FALSE(motion.tick(4020));  // 已到目标，保持不动

    motion.command(YawAction::Right, PitchAction::Center);
    TEST_ASSERT_TRUE(motion.tick(4040));
    TEST_ASSERT_EQUAL_INT(cfg::kYawLeft + 1, motion.yaw());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_touch_short_press);
    RUN_TEST(test_touch_long_press_once);
    RUN_TEST(test_touch_long_press_at_release_boundary);
    RUN_TEST(test_touch_time_wrap);
    RUN_TEST(test_ir_majority_requires_four_of_five);
    RUN_TEST(test_ir_both_sides);
    RUN_TEST(test_behavior_follows_infrared);
    RUN_TEST(test_behavior_lost_holds_then_centers);
    RUN_TEST(test_behavior_touch_priority_and_long_press);
    RUN_TEST(test_behavior_touch_wait_then_center);
    RUN_TEST(test_behavior_requires_clear_window_after_touch);
    RUN_TEST(test_motion_speed_and_limits);
    return UNITY_END();
}
