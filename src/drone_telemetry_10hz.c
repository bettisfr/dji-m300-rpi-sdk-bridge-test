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

static T_DjiReturnCode Subscribe(E_DjiFcSubscriptionTopic topic, E_DjiDataSubscriptionTopicFreq frequency)
{
    return DjiFcSubscription_SubscribeTopic(topic, frequency, NULL);
}

T_DjiReturnCode DjiRpi_RunTelemetry(void)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiAircraftInfoBaseInfo aircraftInfo = {0};
    T_DjiFcSubscriptionQuaternion quaternion = {0};
    T_DjiFcSubscriptionPositionFused fusedPosition = {0};
    T_DjiFcSubscriptionRtkPosition rtkPosition = {0};
    T_DjiFcSubscriptionHeightFusion heightFusion = 0;
    T_DjiFcSubscriptionRtkPositionInfo rtkStatus = 0;
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
        USER_LOG_ERROR("Telemetry: subscription init failed, error code 0x%08X", returnCode);
        return returnCode;
    }

    returnCode = Subscribe(DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION, DJI_DATA_SUBSCRIPTION_TOPIC_10_HZ);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) goto cleanup;
    returnCode = Subscribe(DJI_FC_SUBSCRIPTION_TOPIC_POSITION_FUSED, DJI_DATA_SUBSCRIPTION_TOPIC_10_HZ);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) goto cleanup;
    returnCode = Subscribe(DJI_FC_SUBSCRIPTION_TOPIC_HEIGHT_FUSION, DJI_DATA_SUBSCRIPTION_TOPIC_10_HZ);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) goto cleanup;
    returnCode = Subscribe(DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION, DJI_DATA_SUBSCRIPTION_TOPIC_1_HZ);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) goto cleanup;
    returnCode = Subscribe(DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION_INFO, DJI_DATA_SUBSCRIPTION_TOPIC_1_HZ);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) goto cleanup;
    printf(
        "fc_timestamp_ms,drone_model,firmware,bearing_deg,"
        "fused_lat_deg,fused_lon_deg,fused_alt_m,height_fusion_m,"
        "rtk_lat_deg,rtk_lon_deg,rtk_h_m,rtk_status\n");
    fflush(stdout);

    while (!s_shouldStop) {
        osalHandler->TaskSleepMs(100);
        returnCode = DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION, (uint8_t *) &quaternion, sizeof(quaternion), &timestamp);
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS ||
            timestamp.millisecond == 0 || timestamp.millisecond == lastTimestampMs) {
            continue;
        }
        lastTimestampMs = timestamp.millisecond;

        DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_POSITION_FUSED, (uint8_t *) &fusedPosition, sizeof(fusedPosition), NULL);
        DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_HEIGHT_FUSION, (uint8_t *) &heightFusion, sizeof(heightFusion), NULL);
        DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION, (uint8_t *) &rtkPosition, sizeof(rtkPosition), NULL);
        DjiFcSubscription_GetLatestValueOfTopic(
            DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION_INFO, (uint8_t *) &rtkStatus, sizeof(rtkStatus), NULL);
        printf(
            "%u,\"%s\",\"%s\",%.1f,%.8f,%.8f,%.3f,%.3f,"
            "%.8f,%.8f,%.3f,%u\n",
            timestamp.millisecond,
            AircraftTypeName(aircraftInfo.aircraftType),
            USER_AIRCRAFT_FIRMWARE,
            QuaternionToBearing(&quaternion),
            fusedPosition.latitude * 180.0 / M_PI,
            fusedPosition.longitude * 180.0 / M_PI,
            fusedPosition.altitude,
            heightFusion,
            rtkPosition.latitude,
            rtkPosition.longitude,
            rtkPosition.hfsl,
            rtkStatus);
        fflush(stdout);
    }

cleanup:
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_QUATERNION);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_POSITION_FUSED);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_HEIGHT_FUSION);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION);
    DjiFcSubscription_UnSubscribeTopic(DJI_FC_SUBSCRIPTION_TOPIC_RTK_POSITION_INFO);
    DjiFcSubscription_DeInit();
    return returnCode;
}
