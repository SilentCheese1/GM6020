#include "ControlTask.h"
#include "SineConfig.h"
#include "can.h"
#include "tim.h"

extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == SINE_TIMER_INSTANCE) MainTask();
}

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if (hcan->Instance != CAN1) return;
    CAN_RxHeaderTypeDef header = {};
    uint8_t data[8] = {0};
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, data) == HAL_OK &&
        header.IDE == CAN_ID_STD && header.RTR == CAN_RTR_DATA && header.DLC == 8) {
        CanFeedbackCallback(header.StdId, data);
    }
}
