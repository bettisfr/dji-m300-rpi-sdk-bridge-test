#include "hal_uart.h"
#include "telemetry.h"

#include "dji_core.h"
#include "dji_logger.h"
#include "dji_platform.h"
#include "dji_sdk_app_info.h"
#include "osal/osal.h"
#include "osal/osal_fs.h"

#include <stdio.h>
#include <string.h>

static T_DjiReturnCode PrintConsole(const uint8_t *data, uint16_t dataLen)
{
    return fwrite(data, 1, dataLen, stdout) == dataLen
               ? DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS
               : DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
}

static T_DjiReturnCode RegisterPlatformHandlers(void)
{
    T_DjiOsalHandler osalHandler = {
        .TaskCreate = Osal_TaskCreate,
        .TaskDestroy = Osal_TaskDestroy,
        .TaskSleepMs = Osal_TaskSleepMs,
        .MutexCreate = Osal_MutexCreate,
        .MutexDestroy = Osal_MutexDestroy,
        .MutexLock = Osal_MutexLock,
        .MutexUnlock = Osal_MutexUnlock,
        .SemaphoreCreate = Osal_SemaphoreCreate,
        .SemaphoreDestroy = Osal_SemaphoreDestroy,
        .SemaphoreWait = Osal_SemaphoreWait,
        .SemaphoreTimedWait = Osal_SemaphoreTimedWait,
        .SemaphorePost = Osal_SemaphorePost,
        .Malloc = Osal_Malloc,
        .Free = Osal_Free,
        .GetTimeMs = Osal_GetTimeMs,
        .GetTimeUs = Osal_GetTimeUs,
        .GetRandomNum = Osal_GetRandomNum,
    };
    T_DjiHalUartHandler uartHandler = {
        .UartInit = HalUart_Init,
        .UartDeInit = HalUart_DeInit,
        .UartWriteData = HalUart_WriteData,
        .UartReadData = HalUart_ReadData,
        .UartGetStatus = HalUart_GetStatus,
    };
    T_DjiFileSystemHandler fileSystemHandler = {
        .FileOpen = Osal_FileOpen,
        .FileClose = Osal_FileClose,
        .FileWrite = Osal_FileWrite,
        .FileRead = Osal_FileRead,
        .FileSync = Osal_FileSync,
        .FileSeek = Osal_FileSeek,
        .DirOpen = Osal_DirOpen,
        .DirClose = Osal_DirClose,
        .DirRead = Osal_DirRead,
        .Mkdir = Osal_Mkdir,
        .Unlink = Osal_Unlink,
        .Rename = Osal_Rename,
        .Stat = Osal_Stat,
    };
    T_DjiLoggerConsole console = {
        .func = PrintConsole,
        .consoleLevel = DJI_LOGGER_CONSOLE_LOG_LEVEL_WARN,
        .isSupportColor = true,
    };
    T_DjiReturnCode returnCode;

    returnCode = DjiPlatform_RegOsalHandler(&osalHandler);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    returnCode = DjiPlatform_RegHalUartHandler(&uartHandler);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    returnCode = DjiPlatform_RegFileSystemHandler(&fileSystemHandler);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    return DjiLogger_AddConsole(&console);
}

static void FillUserInfo(T_DjiUserInfo *userInfo)
{
    memset(userInfo, 0, sizeof(*userInfo));
    snprintf(userInfo->appName, sizeof(userInfo->appName), "%s", USER_APP_NAME);
    snprintf(userInfo->appId, sizeof(userInfo->appId), "%s", USER_APP_ID);
    snprintf(userInfo->appKey, sizeof(userInfo->appKey), "%s", USER_APP_KEY);
    snprintf(userInfo->appLicense, sizeof(userInfo->appLicense), "%s", USER_APP_LICENSE);
    snprintf(userInfo->developerAccount, sizeof(userInfo->developerAccount), "%s", USER_DEVELOPER_ACCOUNT);
    snprintf(userInfo->baudRate, sizeof(userInfo->baudRate), "%s", USER_BAUD_RATE);
}

int main(void)
{
    T_DjiFirmwareVersion payloadVersion = {
        .majorVersion = 1,
        .minorVersion = 0,
        .modifyVersion = 0,
        .debugVersion = 0,
    };
    T_DjiUserInfo userInfo;
    T_DjiReturnCode returnCode;

    returnCode = RegisterPlatformHandlers();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Platform initialization failed: 0x%08lX\n", returnCode);
        return 1;
    }

    FillUserInfo(&userInfo);
    returnCode = DjiCore_Init(&userInfo);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "PSDK initialization failed: 0x%08lX\n", returnCode);
        return 1;
    }

    DjiCore_SetAlias("RPi5 Telemetry");
    DjiCore_SetFirmwareVersion(payloadVersion);
    DjiCore_SetSerialNumber("RPI5-TELEMETRY");

    returnCode = DjiCore_ApplicationStart();
    if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        returnCode = DjiRpi_RunTelemetry();
    }

    DjiCore_DeInit();
    return returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS ? 0 : 1;
}
