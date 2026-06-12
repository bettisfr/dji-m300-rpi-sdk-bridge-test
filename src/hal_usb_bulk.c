#include "hal_usb_bulk.h"

#include <libusb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USB_WRITE_TIMEOUT_MS 1000
#define USB_READ_TIMEOUT_MS 0

typedef struct {
    libusb_context *context;
    libusb_device_handle *device;
    T_DjiHalUsbBulkInfo info;
    bool kernelDriverDetached;
} T_UsbBulkHandle;

static T_DjiReturnCode UsbError(const char *operation, int error)
{
    fprintf(stderr, "%s: %s (%d)\n", operation, libusb_error_name(error), error);
    return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
}

T_DjiReturnCode HalUsbBulk_Init(
    T_DjiHalUsbBulkInfo usbBulkInfo,
    T_DjiUsbBulkHandle *usbBulkHandle)
{
    T_UsbBulkHandle *handle;
    int result;

    if (usbBulkHandle == NULL || !usbBulkInfo.isUsbHost) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_MEMORY_ALLOC_FAILED;
    }
    handle->info = usbBulkInfo;

    result = libusb_init(&handle->context);
    if (result != LIBUSB_SUCCESS) {
        free(handle);
        return UsbError("Initialize USB bulk", result);
    }

    handle->device = libusb_open_device_with_vid_pid(
        handle->context,
        usbBulkInfo.vid,
        usbBulkInfo.pid);
    if (handle->device == NULL) {
        fprintf(
            stderr,
            "Open USB bulk device %04x:%04x failed\n",
            usbBulkInfo.vid,
            usbBulkInfo.pid);
        libusb_exit(handle->context);
        free(handle);
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }

    result = libusb_kernel_driver_active(
        handle->device,
        usbBulkInfo.channelInfo.interfaceNum);
    if (result == 1) {
        result = libusb_detach_kernel_driver(
            handle->device,
            usbBulkInfo.channelInfo.interfaceNum);
        if (result != LIBUSB_SUCCESS) {
            libusb_close(handle->device);
            libusb_exit(handle->context);
            free(handle);
            return UsbError("Detach USB kernel driver", result);
        }
        handle->kernelDriverDetached = true;
    }

    result = libusb_claim_interface(
        handle->device,
        usbBulkInfo.channelInfo.interfaceNum);
    if (result != LIBUSB_SUCCESS) {
        if (handle->kernelDriverDetached) {
            libusb_attach_kernel_driver(
                handle->device,
                usbBulkInfo.channelInfo.interfaceNum);
        }
        libusb_close(handle->device);
        libusb_exit(handle->context);
        free(handle);
        return UsbError("Claim USB bulk interface", result);
    }

    printf(
        "USB bulk ready: %04x:%04x interface %u, IN 0x%02x, OUT 0x%02x\n",
        usbBulkInfo.vid,
        usbBulkInfo.pid,
        usbBulkInfo.channelInfo.interfaceNum,
        usbBulkInfo.channelInfo.endPointIn,
        usbBulkInfo.channelInfo.endPointOut);
    *usbBulkHandle = handle;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUsbBulk_DeInit(T_DjiUsbBulkHandle usbBulkHandle)
{
    T_UsbBulkHandle *handle = usbBulkHandle;

    if (handle == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    libusb_release_interface(
        handle->device,
        handle->info.channelInfo.interfaceNum);
    if (handle->kernelDriverDetached) {
        libusb_attach_kernel_driver(
            handle->device,
            handle->info.channelInfo.interfaceNum);
    }
    libusb_close(handle->device);
    libusb_exit(handle->context);
    free(handle);
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUsbBulk_WriteData(
    T_DjiUsbBulkHandle usbBulkHandle,
    const uint8_t *buf,
    uint32_t len,
    uint32_t *realLen)
{
    T_UsbBulkHandle *handle = usbBulkHandle;
    int transferred = 0;
    int result;

    if (handle == NULL || buf == NULL || realLen == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    result = libusb_bulk_transfer(
        handle->device,
        handle->info.channelInfo.endPointOut,
        (unsigned char *) buf,
        (int) len,
        &transferred,
        USB_WRITE_TIMEOUT_MS);
    if (result != LIBUSB_SUCCESS) {
        return UsbError("Write USB bulk data", result);
    }
    *realLen = (uint32_t) transferred;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUsbBulk_ReadData(
    T_DjiUsbBulkHandle usbBulkHandle,
    uint8_t *buf,
    uint32_t len,
    uint32_t *realLen)
{
    T_UsbBulkHandle *handle = usbBulkHandle;
    int transferred = 0;
    int result;

    if (handle == NULL || buf == NULL || realLen == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }
    result = libusb_bulk_transfer(
        handle->device,
        handle->info.channelInfo.endPointIn,
        buf,
        (int) len,
        &transferred,
        USB_READ_TIMEOUT_MS);
    if (result != LIBUSB_SUCCESS) {
        return UsbError("Read USB bulk data", result);
    }
    *realLen = (uint32_t) transferred;
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

T_DjiReturnCode HalUsbBulk_GetDeviceInfo(
    T_DjiHalUsbBulkDeviceInfo *deviceInfo)
{
    if (deviceInfo == NULL) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    /*
     * This callback is only consumed when Linux is the USB device. The RPi5
     * setup is the USB host, so DjiCore supplies the E-Port information to
     * HalUsbBulk_Init instead.
     */
    memset(deviceInfo, 0, sizeof(*deviceInfo));
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
