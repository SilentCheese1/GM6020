#include "ControlTask.h"
#include "SineConfig.h"
#include "Gm6020.h"
#include "Pid.h"
#include "can.h"
#include "tim.h"
#include "iwdg.h"
#include <cmath>

extern "C" {
volatile uint32_t gm6020_control_ticks = 0;
volatile uint8_t gm6020_feedback_ok = 0;
volatile uint32_t gm6020_tx_fail_count = 0;
volatile float gm6020_speed_ref_rad_s = 0.0f;
volatile float gm6020_speed_fdb_rad_s = 0.0f;
volatile float gm6020_speed_filtered_rad_s = 0.0f;
volatile float gm6020_current_fdb_a = 0.0f;
volatile float gm6020_position_ref_rad = 0.0f;
volatile float gm6020_position_fdb_rad = 0.0f;
#if GM6020_CURRENT_CONTROL
volatile int16_t gm6020_current_command = 0;
#else
volatile int16_t gm6020_voltage_command = 0;
#endif
#if GM6020_DIRECTION_DIAG
volatile uint32_t gm6020_direction_diag_ticks = 0;
volatile float gm6020_direction_diag_peak_speed_rad_s = 0.0f;
#endif
}

namespace {
constexpr float kPi = 3.14159265358979323846f;
#if GM6020_CURRENT_CONTROL
constexpr float kCommandLimit = SINE_MAX_CURRENT_COMMAND;
constexpr float kSpeedKp = SINE_CURRENT_SPEED_KP;
constexpr float kSpeedKi = SINE_CURRENT_SPEED_KI;
constexpr float kSpeedKd = SINE_CURRENT_SPEED_KD;
#define GM6020_COMMAND_VAR gm6020_current_command
#else
constexpr float kCommandLimit = SINE_MAX_VOLTAGE_COMMAND;
constexpr float kSpeedKp = SINE_SPEED_KP;
constexpr float kSpeedKi = SINE_SPEED_KI;
constexpr float kSpeedKd = SINE_SPEED_KD;
#define GM6020_COMMAND_VAR gm6020_voltage_command
#endif
static_assert(SINE_MOTOR_ID >= 1 && SINE_MOTOR_ID <= 7, "GM6020 ID must be 1..7");
static_assert(SINE_DEFAULT_MODE <= 1, "mode must be 0 or 1");
static_assert(SINE_FREQUENCY_HZ > 0.0f, "frequency must be positive");
static_assert(SINE_MAX_VOLTAGE_COMMAND > 0 && SINE_MAX_VOLTAGE_COMMAND <= 25000,
              "invalid voltage limit");
static_assert(SINE_MAX_CURRENT_COMMAND > 0 && SINE_MAX_CURRENT_COMMAND <= 16384,
              "invalid current limit");
#if GM6020_CURRENT_CONTROL
static_assert(SINE_STATIC_FRICTION_COMMAND >= 0 &&
              SINE_MAX_CURRENT_COMMAND + SINE_STATIC_FRICTION_COMMAND <= 16384,
              "invalid static friction command");
#endif
#if GM6020_DIRECTION_DIAG
static_assert(GM6020_CURRENT_CONTROL, "direction diagnostic requires current mode");
#endif

Gm6020 motor(SINE_MOTOR_ID);
Pid speed_pid(kSpeedKp, kSpeedKi, kSpeedKd,
              kCommandLimit, -kCommandLimit,
              SINE_CONTROL_DT_S);
volatile uint32_t feedback_ms = 0;
volatile bool feedback_seen = false;
volatile uint8_t requested_mode = SINE_DEFAULT_MODE;
volatile uint8_t active_mode = 255u;
float phase_s = 0.0f;
float position_origin = 0.0f;
float filtered_speed = 0.0f;
bool speed_filter_valid = false;

float clamp(float x, float limit) {
    return x > limit ? limit : (x < -limit ? -limit : x);
}

bool sendControl(int16_t value) {
    uint8_t data[8] = {0};
    motor.setCommand(value);
    motor.encode(data);
    CAN_TxHeaderTypeDef header = {};
    header.StdId = motor.txId();
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = 8;
    uint32_t mailbox = 0;
    return HAL_CAN_AddTxMessage(&hcan1, &header, data, &mailbox) == HAL_OK;
}
}

extern "C" void ControlTaskInit(void) {
    CAN_FilterTypeDef filter = {};
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = static_cast<uint16_t>(motor.rxId() << 5);
    filter.FilterIdLow = 0;
    filter.FilterMaskIdHigh = 0xFFE0u; // 精确匹配 11 位标准 ID
    filter.FilterMaskIdLow = 0x0006u;  // 匹配 IDE 与 RTR 位
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK ||
        HAL_CAN_Start(&hcan1) != HAL_OK ||
        HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK ||
        HAL_TIM_Base_Start_IT(&SINE_TIMER_HANDLE) != HAL_OK) {
        Error_Handler(); // 不喂狗，复位；电机需要独立急停措施
    }
}

extern "C" void ControlTaskSetMode(uint8_t mode) {
    if (mode <= 1u) requested_mode = mode;
}

extern "C" void CanFeedbackCallback(uint32_t std_id, const uint8_t *data) {
    if (std_id != motor.rxId() || data == nullptr) return;
    motor.decode(data);
    feedback_ms = HAL_GetTick();
    feedback_seen = true;
}

extern "C" void MainTask(void) {
    ++gm6020_control_ticks;
    const uint32_t now = HAL_GetTick();
    const bool fresh = feedback_seen && (now - feedback_ms) <= SINE_FEEDBACK_TIMEOUT_MS;
    gm6020_feedback_ok = fresh ? 1u : 0u;
    gm6020_speed_fdb_rad_s = feedback_seen ? motor.vel() : 0.0f;
    gm6020_current_fdb_a = feedback_seen ? motor.current() : 0.0f;
    gm6020_position_fdb_rad = feedback_seen ? motor.angle() : 0.0f;
    if (!fresh) {
        speed_pid.reset();
        speed_filter_valid = false;
        filtered_speed = 0.0f;
        gm6020_speed_filtered_rad_s = 0.0f;
        active_mode = 255u;
        phase_s = 0.0f;
        gm6020_speed_ref_rad_s = 0.0f;
        gm6020_position_ref_rad = gm6020_position_fdb_rad;
        GM6020_COMMAND_VAR = 0;
        if (!sendControl(0)) ++gm6020_tx_fail_count;
        else gm6020_tx_fail_count = 0;
        return;
    }

    const float raw_speed = motor.vel();
    if (!speed_filter_valid) {
        filtered_speed = raw_speed;
        speed_filter_valid = true;
    } else {
        constexpr float kAlpha = SINE_CONTROL_DT_S /
            (SINE_SPEED_FILTER_TAU_S + SINE_CONTROL_DT_S);
        filtered_speed += kAlpha * (raw_speed - filtered_speed);
    }
    gm6020_speed_filtered_rad_s = filtered_speed;

#if GM6020_DIRECTION_DIAG
    // One 200 ms, +800 raw current pulse after the first valid feedback.
    // A watchdog reset re-arms the test; keep motor power disconnected while flashing.
    const uint32_t diag_tick = gm6020_direction_diag_ticks;
    if (diag_tick < 500u) {
        const float speed = motor.vel();
        if (std::fabs(speed) > std::fabs(gm6020_direction_diag_peak_speed_rad_s)) {
            gm6020_direction_diag_peak_speed_rad_s = speed;
        }
        gm6020_direction_diag_ticks = diag_tick + 1u;
    }
    gm6020_speed_ref_rad_s = 0.0f;
    gm6020_position_ref_rad = motor.angle();
    GM6020_COMMAND_VAR = diag_tick < 200u ? 800 : 0;
    if (!sendControl(GM6020_COMMAND_VAR)) ++gm6020_tx_fail_count;
    else gm6020_tx_fail_count = 0;
    return;
#endif

    const uint8_t mode = requested_mode;
    if (active_mode != mode) {
        speed_pid.reset();
        position_origin = motor.angle();
        phase_s = 0.0f;
        active_mode = mode;
    }

    const float omega = 2.0f * kPi * SINE_FREQUENCY_HZ;
    const float phase = omega * phase_s;
    float target_speed;
    float target_position = motor.angle();
    if (mode == 0u) {
        target_speed = SINE_SPEED_AMPLITUDE_RAD_S * std::sin(phase);
    } else {
        target_position = position_origin +
            SINE_POSITION_AMPLITUDE_RAD * std::sin(phase);
        const float feedforward = SINE_POSITION_AMPLITUDE_RAD * omega * std::cos(phase);
        target_speed = feedforward + SINE_POSITION_KP * (target_position - motor.angle());
    }
    target_speed = clamp(target_speed, SINE_MAX_SPEED_RAD_S);
    gm6020_speed_ref_rad_s = target_speed;
    gm6020_position_ref_rad = target_position;
    float command = speed_pid.calc(target_speed, filtered_speed);
#if GM6020_CURRENT_CONTROL
    // 低速时平滑加入摩擦补偿，过零时补偿为零，避免方向反转时跳变。
    const float friction_blend = clamp(target_speed / 0.5f, 1.0f);
    command += SINE_STATIC_FRICTION_COMMAND * friction_blend;
    command = clamp(command, kCommandLimit + SINE_STATIC_FRICTION_COMMAND);
#endif
    GM6020_COMMAND_VAR = static_cast<int16_t>(command);
    if (!sendControl(GM6020_COMMAND_VAR)) ++gm6020_tx_fail_count;
    else gm6020_tx_fail_count = 0;

    phase_s += SINE_CONTROL_DT_S;
    if (phase_s >= 1.0f / SINE_FREQUENCY_HZ) phase_s -= 1.0f / SINE_FREQUENCY_HZ;
}

extern "C" void ControlTaskService(void) {
    static uint32_t last_ticks = 0;
    static uint32_t last_advance_ms = 0;
    const uint32_t now = HAL_GetTick();
    const uint32_t ticks = gm6020_control_ticks;
    if (ticks != last_ticks) {
        last_ticks = ticks;
        last_advance_ms = now;
    }
    // 仅在主循环活着、控制定时器在跑且 CAN 发送未持续故障时喂狗。
    if (ticks != 0 && (now - last_advance_ms) < SINE_TIMER_TIMEOUT_MS &&
        gm6020_tx_fail_count < 20u) {
        HAL_IWDG_Refresh(&hiwdg);
    }
}
