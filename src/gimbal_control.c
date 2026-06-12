#include "gimbal_control.h"

#include "dji_fc_subscription.h"
#include "dji_gimbal_manager.h"
#include "dji_platform.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GIMBAL_MOUNT_POSITION DJI_MOUNT_POSITION_PAYLOAD_PORT_NO1

static T_DjiReturnCode ReadGimbalAngles(T_DjiFcSubscriptionGimbalAngles *angles)
{
    T_DjiDataTimestamp timestamp = {0};

    return DjiFcSubscription_GetLatestValueOfTopic(
        DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES,
        (uint8_t *) angles,
        sizeof(*angles),
        &timestamp);
}

static void PrintGimbalAngles(
    const char *label,
    const T_DjiFcSubscriptionGimbalAngles *angles)
{
    printf(
        "%s: pitch %.1f, roll %.1f, yaw %.1f deg\n",
        label,
        angles->x,
        angles->y,
        angles->z);
}

static T_DjiReturnCode InitGimbalControl(void)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiReturnCode returnCode;

    returnCode = DjiFcSubscription_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    returnCode = DjiFcSubscription_SubscribeTopic(
        DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES,
        DJI_DATA_SUBSCRIPTION_TOPIC_10_HZ,
        NULL);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        DjiFcSubscription_DeInit();
        return returnCode;
    }

    osalHandler->TaskSleepMs(500);
    returnCode = DjiGimbalManager_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
        DjiFcSubscription_DeInit();
        return returnCode;
    }

    returnCode = DjiGimbalManager_SetMode(
        GIMBAL_MOUNT_POSITION,
        DJI_GIMBAL_MODE_YAW_FOLLOW);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        DjiGimbalManager_Deinit();
        DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
        DjiFcSubscription_DeInit();
    }
    return returnCode;
}

static void DeInitGimbalControl(void)
{
    DjiGimbalManager_Deinit();
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
}

static bool ParseFloat(const char *text, float *value)
{
    char *end;

    errno = 0;
    *value = strtof(text, &end);
    while (*end == ' ' || *end == '\t' || *end == '\n') {
        end++;
    }
    return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

T_DjiReturnCode DjiRpi_SetGimbalPitch(float targetPitchDegrees)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiFcSubscriptionGimbalAngles before = {0};
    T_DjiFcSubscriptionGimbalAngles after = {0};
    T_DjiGimbalManagerRotation rotation = {
        .rotationMode = DJI_GIMBAL_ROTATION_MODE_RELATIVE_ANGLE,
        .roll = 0.0f,
        .yaw = 0.0f,
        .time = 3.0,
    };
    T_DjiReturnCode returnCode;

    if (targetPitchDegrees < -90.0f || targetPitchDegrees > 30.0f) {
        fprintf(stderr, "Pitch target must be between -90 and 30 degrees.\n");
        return DJI_ERROR_SYSTEM_MODULE_CODE_INVALID_PARAMETER;
    }

    returnCode = InitGimbalControl();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    returnCode = ReadGimbalAngles(&before);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        DeInitGimbalControl();
        return returnCode;
    }

    rotation.pitch = targetPitchDegrees - before.x;
    PrintGimbalAngles("Gimbal before", &before);
    printf("Command: relative pitch %.1f deg -> target %.1f deg\n", rotation.pitch, targetPitchDegrees);
    fflush(stdout);

    returnCode = DjiGimbalManager_Rotate(GIMBAL_MOUNT_POSITION, rotation);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Gimbal rotation failed: 0x%08lX\n", returnCode);
        DeInitGimbalControl();
        return returnCode;
    }

    osalHandler->TaskSleepMs(3500);
    returnCode = ReadGimbalAngles(&after);
    if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        PrintGimbalAngles("Gimbal after", &after);
    }
    DeInitGimbalControl();
    return returnCode;
}

T_DjiReturnCode DjiRpi_RunGimbalConsole(void)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    char line[64];
    T_DjiReturnCode returnCode;

    returnCode = InitGimbalControl();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Gimbal initialization failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    while (true) {
        T_DjiFcSubscriptionGimbalAngles before = {0};
        T_DjiFcSubscriptionGimbalAngles after = {0};
        T_DjiGimbalManagerRotation rotation = {
            .rotationMode = DJI_GIMBAL_ROTATION_MODE_RELATIVE_ANGLE,
            .time = 2.0,
        };
        float degrees;
        int axis;

        returnCode = ReadGimbalAngles(&before);
        if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            PrintGimbalAngles("\nCurrent", &before);
        }
        printf("1 pitch | 2 roll | 3 yaw | q quit\nSelect axis: ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL ||
            line[0] == 'q' ||
            line[0] == 'Q') {
            break;
        }

        axis = (int) strtol(line, NULL, 10);
        if (axis < 1 || axis > 3) {
            printf("Invalid selection.\n");
            continue;
        }

        printf("Relative movement in degrees: ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        if (!ParseFloat(line, &degrees) || degrees < -90.0f || degrees > 90.0f) {
            printf("Enter a value between -90 and 90 degrees.\n");
            continue;
        }

        if (axis == 1) {
            rotation.pitch = degrees;
        } else if (axis == 2) {
            rotation.roll = degrees;
        } else {
            rotation.yaw = degrees;
        }

        returnCode = DjiGimbalManager_Rotate(GIMBAL_MOUNT_POSITION, rotation);
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            fprintf(stderr, "Gimbal rotation failed: 0x%08lX\n", returnCode);
            continue;
        }

        osalHandler->TaskSleepMs(2300);
        returnCode = ReadGimbalAngles(&after);
        if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            PrintGimbalAngles("After", &after);
        }
    }

    DeInitGimbalControl();
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
