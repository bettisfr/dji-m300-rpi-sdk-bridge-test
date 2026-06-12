#ifndef DJI_RPI_HAL_USB_BULK_H
#define DJI_RPI_HAL_USB_BULK_H

#include "dji_platform.h"

T_DjiReturnCode HalUsbBulk_Init(
    T_DjiHalUsbBulkInfo usbBulkInfo,
    T_DjiUsbBulkHandle *usbBulkHandle);
T_DjiReturnCode HalUsbBulk_DeInit(T_DjiUsbBulkHandle usbBulkHandle);
T_DjiReturnCode HalUsbBulk_WriteData(
    T_DjiUsbBulkHandle usbBulkHandle,
    const uint8_t *buf,
    uint32_t len,
    uint32_t *realLen);
T_DjiReturnCode HalUsbBulk_ReadData(
    T_DjiUsbBulkHandle usbBulkHandle,
    uint8_t *buf,
    uint32_t len,
    uint32_t *realLen);
T_DjiReturnCode HalUsbBulk_GetDeviceInfo(
    T_DjiHalUsbBulkDeviceInfo *deviceInfo);

#endif
