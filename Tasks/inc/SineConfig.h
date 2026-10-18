#ifndef SINE_CONFIG_H
#define SINE_CONFIG_H

// 修改这里的数值后重新编译。电机拨码 ID 需与此一致。
#define SINE_MOTOR_ID                 1u
#ifndef SINE_DEFAULT_MODE
#define SINE_DEFAULT_MODE             0u    // 0: 速度；1: 位置
#endif
#define SINE_FREQUENCY_HZ             0.20f // 周期 5 s
#define SINE_SPEED_AMPLITUDE_RAD_S    3.0f
#define SINE_POSITION_AMPLITUDE_RAD   0.50f
#define SINE_MAX_SPEED_RAD_S          5.0f
#define SINE_MAX_VOLTAGE_COMMAND      12000.0f // 协议上限为 25000
#define SINE_MAX_CURRENT_COMMAND      1000.0f  // PID 部分的电流指令限幅，约 0.18 A
#define SINE_FEEDBACK_TIMEOUT_MS      50u
#define SINE_TIMER_TIMEOUT_MS         100u
// 两种模式的速度内环分别调参；CMake 的 GM6020_DEMO_MODE 选择速度/位置。
#if SINE_DEFAULT_MODE == 0
#define SINE_SPEED_FILTER_TAU_S       0.050f // 速度试车：50 ms
#define SINE_CURRENT_SPEED_KP         375.0f
#define SINE_CURRENT_SPEED_KI         0.0f
#define SINE_CURRENT_SPEED_KD         0.0f
#define SINE_STATIC_FRICTION_COMMAND  350.0f // 速度模式起步补偿；原始指令单位，约 0.06 A
#else
#define SINE_SPEED_FILTER_TAU_S       0.050f // 位置模式先采用已试过的速度内环：50 ms
#define SINE_CURRENT_SPEED_KP         375.0f
#define SINE_CURRENT_SPEED_KI         0.0f
#define SINE_CURRENT_SPEED_KD         0.0f
#define SINE_STATIC_FRICTION_COMMAND  350.0f // 位置模式速度内环起步补偿；原始指令单位
#endif

// 初始值仅用于低速试车；需按负载、供电和实际反馈调参。
#define SINE_SPEED_KP                 1500.0f
#define SINE_SPEED_KI                 250.0f
#define SINE_SPEED_KD                 0.0f
#define SINE_POSITION_KP              6.0f

// 为 dartrack1.ioc 编译时定义 SINE_USE_DARTRACK1；其控制定时器是 TIM3。
#ifdef SINE_USE_DARTRACK1
#define SINE_TIMER_HANDLE htim3
#define SINE_TIMER_INSTANCE TIM3
#define SINE_CONTROL_DT_S 0.002f
#else
#define SINE_TIMER_HANDLE htim6
#define SINE_TIMER_INSTANCE TIM6
#define SINE_CONTROL_DT_S 0.001f
#endif

#endif
