#ifndef DJI_RPI_HAL_UART_H
#define DJI_RPI_HAL_UART_H

#include "dji_platform.h"

T_DjiReturnCode HalUart_Init(
    E_DjiHalUartNum uartNum,
    uint32_t baudRate,
    T_DjiUartHandle *uartHandle);
T_DjiReturnCode HalUart_DeInit(T_DjiUartHandle uartHandle);
T_DjiReturnCode HalUart_WriteData(
    T_DjiUartHandle uartHandle,
    const uint8_t *buf,
    uint32_t len,
    uint32_t *realLen);
T_DjiReturnCode HalUart_ReadData(
    T_DjiUartHandle uartHandle,
    uint8_t *buf,
    uint32_t len,
    uint32_t *realLen);
T_DjiReturnCode HalUart_GetStatus(E_DjiHalUartNum uartNum, T_DjiUartStatus *status);

#endif
