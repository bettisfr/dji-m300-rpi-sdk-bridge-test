#include "telemetry.h"

#include "dji_aircraft_info.h"
#include "dji_fc_subscription.h"
#include "dji_logger.h"
#include "dji_platform.h"
#include "dji_sdk_app_info.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t s_shouldStop;

static void HandleSignal(int signalNumber)
{
    (void) signalNumber;
    s_shouldStop = 1;
}

static const char *AircraftTypeName(E_DjiAircraftType aircraftType)
{
    switch (aircraftType) {
        case DJI_AIRCRAFT_TYPE_M300_RTK:
            return "Matrice 300 RTK";
        case DJI_AIRCRAFT_TYPE_M350_RTK:
            return "Matrice 350 RTK";
        default:
            return "Unknown";
    }
}

static double QuaternionToBearing(const T_DjiFcSubscriptionQuaternion *quaternion)
{
    double yaw = atan2(
        2.0 * (quaternion->q1 * quaternion->q2 + quaternion->q0 * quaternion->q3),
        1.0 - 2.0 * (quaternion->q2 * quaternion->q2 + quaternion->q3 * quaternion->q3));
    double bearing = yaw * 180.0 / M_PI;

    return bearing < 0.0 ? bearing + 360.0 : bearing;
}

T_DjiReturnCode DjiRpi_RunTelemetry(void)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiAircraftInfoBaseInfo aircraftInfo = {0};
    T_DjiFcSubscriptionGimbalAngles angles = {0};
    T_DjiFcSubscriptionQuaternion quaternion = {0};
    T_DjiFcSubscriptionWholeBatteryInfo battery = {0};
    T_DjiDataTimestamp timestamp = {0};
    uint32_t lastTimestampMs = 0;
    T_DjiReturnCode returnCode;

    signal(SIGINT, HandleSignal);
    signal(SIGTERM, HandleSignal);

    returnCode = DjiAircraftInfo_GetBaseInfo(&aircraftInfo);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        USER_LOG_ERROR("Telemetry: aircraft info failed, error code 0x%08X", returnCode);
        return returnCode;
    }

    returnCode = DjiFcSubscription_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        USER_LOG_ERROR("Gimbal angles: subscription init failed, error code 0x%08X", returnCode);
        return returnCode;
    }

    returnCode = DjiFcSubscription_SubscribeTopic(
        DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES,
        DJI_DATA_SUBSCRIPTION_TOPIC_1_HZ,
        NULL);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        USER_LOG_ERROR("Gimbal angles: subscribe failed, error code 0x%08X", returnCode);
        DjiFcSubscription_DeInit();
        return returnCode;
    }

    returnCode = DjiFcSubscription_SubscribeTopic(
        DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION,
        DJI_DATA_SUBSCRIPTION_TOPIC_1_HZ,
        NULL);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        USER_LOG_ERROR("Telemetry: quaternion subscribe failed, error code 0x%08X", returnCode);
        DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
        DjiFcSubscription_DeInit();
        return returnCode;
    }

    returnCode = DjiFcSubscription_SubscribeTopic(
        DJI_FC_SUBSCRIPTION_TOPIC_BATTERY_INFO,
        DJI_DATA_SUBSCRIPTION_TOPIC_1_HZ,
        NULL);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        USER_LOG_ERROR("Telemetry: battery subscribe failed, error code 0x%08X", returnCode);
        DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
        DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION);
        DjiFcSubscription_DeInit();
        return returnCode;
    }

    printf(
        "timestamp_ms,drone_model,firmware,bearing_deg,gimbal_roll_deg,"
        "gimbal_pitch_deg,gimbal_yaw_deg,battery_percent,battery_voltage_v\n");
    fflush(stdout);

    while (!s_shouldStop) {
        osalHandler->TaskSleepMs(1000);
        returnCode = DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES,
            (uint8_t *) &angles,
            sizeof(angles),
            &timestamp);
        if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            returnCode = DjiFcSubscription_GetLatestValueOfTopic(
                DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION,
                (uint8_t *) &quaternion,
                sizeof(quaternion),
                NULL);
        }
        if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            returnCode = DjiFcSubscription_GetLatestValueOfTopic(
                DJI_FC_SUBSCRIPTION_TOPIC_BATTERY_INFO,
                (uint8_t *) &battery,
                sizeof(battery),
                NULL);
        }
        if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            if (timestamp.millisecond == 0 || timestamp.millisecond == lastTimestampMs) {
                continue;
            }
            lastTimestampMs = timestamp.millisecond;
            printf(
                "%u,\"%s\",\"%s\",%.1f,%.1f,%.1f,%.1f,%u,%.3f\n",
                timestamp.millisecond,
                AircraftTypeName(aircraftInfo.aircraftType),
                USER_AIRCRAFT_FIRMWARE,
                QuaternionToBearing(&quaternion),
                angles.y,
                angles.x,
                angles.z,
                battery.percentage,
                (double) battery.voltage / 1000.0);
            fflush(stdout);
        } else {
            fprintf(stderr, "Telemetry: read failed, error code 0x%08lX\n", returnCode);
        }
    }

    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_GIMBAL_ANGLES);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_BATTERY_INFO);
    DjiFcSubscription_DeInit();
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}
