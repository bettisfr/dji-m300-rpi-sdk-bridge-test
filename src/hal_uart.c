#include "hal_uart.h"

#include "dji_sdk_app_info.h"

#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
    int fd;
} T_UartHandle;

static speed_t BaudRateToSpeed(uint32_t baudRate)
{
    switch (baudRate) {
        case 115200:
            return B115200;
        case 230400:
            return B230400;
        case 460800:
            return B460800;
        case 921600:
            return B921600;
        case 1000000:
            return B1000000;
        default:
            return 0;
    }
}

T_DjiReturnCode HalUart_Init(
    E_DjiHalUartNum uartNum,
    uint32_t baudRate,
    T_DjiUartHandle *uartHandle)
{
    T_UartHandle *handle;
    const char *device;
    struct termios options;
    speed_t speed;

    if (uartHandle == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    if (uartNum == DJI_HAL_UART_NUM_0) {
        device = USER_UART_DEVICE;
    } else if (uartNum == DJI_HAL_UART_NUM_1) {
        device = USER_UART_SECONDARY_DEVICE;
    } else {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    speed = BaudRateToSpeed(baudRate);
    if (speed == 0) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_MEMORY_ALLOC_FAILED;
    }

    handle->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (handle->fd < 0 || tcgetattr(handle->fd, &options) != 0) {
        goto error;
    }

    cfmakeraw(&options);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cflag &= ~CRTSCTS;
    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 0;

    tcflush(handle->fd, TCIFLUSH);
    if (tcsetattr(handle->fd, TCSANOW, &options) != 0) {
        goto error;
    }

    *uartHandle = handle;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;

error:
    if (handle->fd >= 0) {
        close(handle->fd);
    }
    free(handle);
    return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
}

T_DjiReturnCode HalUart_DeInit(T_DjiUartHandle uartHandle)
{
    T_UartHandle *handle = uartHandle;

    if (handle == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    close(handle->fd);
    free(handle);
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUart_WriteData(
    T_DjiUartHandle uartHandle,
    const uint8_t *buf,
    uint32_t len,
    uint32_t *realLen)
{
    T_UartHandle *handle = uartHandle;
    ssize_t written;

    if (handle == NULL || buf == NULL || realLen == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    written = write(handle->fd, buf, len);
    if (written < 0) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }
    *realLen = (uint32_t) written;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUart_ReadData(
    T_DjiUartHandle uartHandle,
    uint8_t *buf,
    uint32_t len,
    uint32_t *realLen)
{
    T_UartHandle *handle = uartHandle;
    ssize_t readLength;

    if (handle == NULL || buf == NULL || realLen == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    readLength = read(handle->fd, buf, len);
    if (readLength < 0) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }
    *realLen = (uint32_t) readLength;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUart_GetStatus(E_DjiHalUartNum uartNum, T_DjiUartStatus *status)
{
    if (status == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    if (uartNum == DJI_HAL_UART_NUM_0) {
        status->isConnect = access(USER_UART_DEVICE, F_OK) == 0;
    } else if (uartNum == DJI_HAL_UART_NUM_1) {
        status->isConnect = access(USER_UART_SECONDARY_DEVICE, F_OK) == 0;
    } else {
        status->isConnect = false;
    }
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

#ifdef DJI_UART_HAS_DEVICE_INFO
T_DjiReturnCode HalUart_GetDeviceInfo(T_DjiHalUartDeviceInfo *deviceInfo)
{
    if (deviceInfo == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    deviceInfo->vid = 0x10C4;
    deviceInfo->pid = 0xEA60;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
#endif
