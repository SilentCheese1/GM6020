#ifndef CONTROLTASK_H
#define CONTROLTASK_H

#include <stdint.h>

// 供 C 中断处理(stm32f4xx_it.c 及 HAL 弱回调)调用的接口
#ifdef __cplusplus
extern "C" {
#endif

// 上电初始化: 配置 CAN 过滤器、启动 CAN 与 1kHz 控制定时器
// 在 main.c 中 MX_XXX_Init() 全部执行完后调用一次
void ControlTaskInit(void);

// 1kHz 控制入口, 在 TIM6 周期中断(HAL_TIM_PeriodElapsedCallback)中调用
void MainTask(void);

// 在主循环中持续调用；仅当 TIM6 按期运行时刷新 IWDG
void ControlTaskService(void);

// 0=正弦速度, 1=正弦位置；切换时清空 PID 并重设时间/位置基准
void ControlTaskSetMode(uint8_t mode);

// CAN 接收回调, 在 HAL_CAN_RxFifo0MsgPendingCallback 中调用
void CanFeedbackCallback(uint32_t std_id, const uint8_t *data);

// Ozone 的 Data Sampling 可直接添加以下全局变量（目标/反馈使用相同单位）。
extern volatile uint32_t gm6020_control_ticks;
extern volatile uint8_t gm6020_feedback_ok;
extern volatile float gm6020_speed_ref_rad_s;
extern volatile float gm6020_speed_fdb_rad_s;
extern volatile float gm6020_position_ref_rad;
extern volatile float gm6020_position_fdb_rad;
#if GM6020_CURRENT_CONTROL
extern volatile int16_t gm6020_current_command; // ±16384 对应 ±3 A
#else
extern volatile int16_t gm6020_voltage_command; // 电压模式原始给定
#endif

#ifdef __cplusplus
}
#endif

#endif // CONTROLTASK_H
