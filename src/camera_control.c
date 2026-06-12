#include "camera_control.h"

#include "dji_camera_manager.h"
#include "dji_platform.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define CAMERA_MOUNT_POSITION DJI_MOUNT_POSITION_PAYLOAD_PORT_NO1

static FILE *s_downloadFile;
static char s_downloadPath[512];
static bool s_downloadComplete;

static T_DjiReturnCode DownloadFileDataCallback(
    T_DjiDownloadFilePacketInfo packetInfo,
    const uint8_t *data,
    uint16_t dataLen)
{
    if (packetInfo.downloadFileEvent == DJI_DOWNLOAD_FILE_EVENT_START ||
        packetInfo.downloadFileEvent == DJI_DOWNLOAD_FILE_EVENT_START_TRANSFER_END) {
        s_downloadFile = fopen(s_downloadPath, "wb");
        if (s_downloadFile == NULL) {
            return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
        }
    }

    if (s_downloadFile == NULL ||
        (dataLen > 0 && fwrite(data, 1, dataLen, s_downloadFile) != dataLen)) {
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }

    if (packetInfo.downloadFileEvent == DJI_DOWNLOAD_FILE_EVENT_END ||
        packetInfo.downloadFileEvent == DJI_DOWNLOAD_FILE_EVENT_START_TRANSFER_END) {
        fclose(s_downloadFile);
        s_downloadFile = NULL;
        s_downloadComplete = true;
        printf(
            "Downloaded %s (%u bytes)\n",
            s_downloadPath,
            packetInfo.fileSize);
    }

    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

static T_DjiReturnCode InitCameraManager(void)
{
    T_DjiCameraManagerFirmwareVersion firmwareVersion = {0};
    E_DjiCameraType cameraType;
    T_DjiReturnCode returnCode;

    returnCode = DjiCameraManager_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Camera manager initialization failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    returnCode = DjiCameraManager_GetCameraType(CAMERA_MOUNT_POSITION, &cameraType);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Get camera type failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    returnCode = DjiCameraManager_GetFirmwareVersion(
        CAMERA_MOUNT_POSITION,
        &firmwareVersion);
    if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        printf(
            "Camera position 1: type %d, firmware %u.%u.%u.%u\n",
            cameraType,
            firmwareVersion.firmware_version[0],
            firmwareVersion.firmware_version[1],
            firmwareVersion.firmware_version[2],
            firmwareVersion.firmware_version[3]);
    } else {
        printf("Camera position 1: type %d\n", cameraType);
    }
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

static T_DjiReturnCode ShootSinglePhoto(void)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    E_DjiCameraManagerWorkMode workMode;
    T_DjiReturnCode returnCode;

    returnCode = DjiCameraManager_SetMode(
        CAMERA_MOUNT_POSITION,
        DJI_CAMERA_MANAGER_WORK_MODE_SHOOT_PHOTO);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS &&
        returnCode != DJI_ERROR_CAMERA_MANAGER_MODULE_CODE_UNSUPPORTED_COMMAND) {
        fprintf(stderr, "Set photo mode failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    osalHandler->TaskSleepMs(1000);
    returnCode = DjiCameraManager_GetMode(CAMERA_MOUNT_POSITION, &workMode);
    if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        printf("Camera work mode: %d\n", workMode);
    }

    returnCode = DjiCameraManager_SetShootPhotoMode(
        CAMERA_MOUNT_POSITION,
        DJI_CAMERA_MANAGER_SHOOT_PHOTO_MODE_SINGLE);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS &&
        returnCode != DJI_ERROR_CAMERA_MANAGER_MODULE_CODE_UNSUPPORTED_COMMAND) {
        fprintf(stderr, "Set single-photo mode failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    osalHandler->TaskSleepMs(500);
    printf("Taking single photo on payload position 1...\n");
    fflush(stdout);

    returnCode = DjiCameraManager_StartShootPhoto(
        CAMERA_MOUNT_POSITION,
        DJI_CAMERA_MANAGER_SHOOT_PHOTO_MODE_SINGLE);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Shoot photo failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    osalHandler->TaskSleepMs(2000);
    printf("Photo command accepted. The image is stored by the H20.\n");
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

static const char *BaseName(const char *fileName)
{
    const char *slash = strrchr(fileName, '/');
    const char *backslash = strrchr(fileName, '\\');
    const char *base = fileName;

    if (slash != NULL && slash + 1 > base) {
        base = slash + 1;
    }
    if (backslash != NULL && backslash + 1 > base) {
        base = backslash + 1;
    }
    return base;
}

static T_DjiReturnCode DownloadLatestPhoto(const char *outputDirectory)
{
    T_DjiCameraManagerFileList fileList = {0};
    T_DjiCameraManagerFileListInfo *latest;
    T_DjiReturnCode returnCode;
    bool rightsObtained = false;

    if (mkdir(outputDirectory, 0755) != 0 && errno != EEXIST) {
        perror("Create photo directory");
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }

    returnCode = DjiCameraManager_RegDownloadFileDataCallback(
        CAMERA_MOUNT_POSITION,
        DownloadFileDataCallback);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Register download callback failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    returnCode = DjiCameraManager_ObtainDownloaderRights(CAMERA_MOUNT_POSITION);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Obtain downloader rights failed: 0x%08lX\n", returnCode);
        return returnCode;
    }
    rightsObtained = true;

    returnCode = DjiCameraManager_DownloadFileList(CAMERA_MOUNT_POSITION, &fileList);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Download file list failed: 0x%08lX\n", returnCode);
        goto cleanup;
    }
    if (fileList.totalCount == 0 || fileList.fileListInfo == NULL) {
        fprintf(stderr, "The H20 media list is empty.\n");
        returnCode = DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
        goto cleanup;
    }

    latest = &fileList.fileListInfo[0];
    for (uint16_t i = 1; i < fileList.totalCount; ++i) {
        if (fileList.fileListInfo[i].fileIndex > latest->fileIndex) {
            latest = &fileList.fileListInfo[i];
        }
    }

    snprintf(
        s_downloadPath,
        sizeof(s_downloadPath),
        "%s/%s",
        outputDirectory,
        BaseName(latest->fileName));
    s_downloadComplete = false;
    printf(
        "Downloading latest media: %s, index %u, size %u bytes\n",
        latest->fileName,
        latest->fileIndex,
        latest->fileSize);
    fflush(stdout);

    returnCode = DjiCameraManager_DownloadFileByIndex(
        CAMERA_MOUNT_POSITION,
        latest->fileIndex);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "Download media failed: 0x%08lX\n", returnCode);
        goto cleanup;
    }
    if (!s_downloadComplete) {
        fprintf(stderr, "Download ended without a complete file event.\n");
        returnCode = DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
        goto cleanup;
    }

cleanup:
    if (s_downloadFile != NULL) {
        fclose(s_downloadFile);
        s_downloadFile = NULL;
    }
    if (rightsObtained) {
        T_DjiReturnCode releaseCode =
            DjiCameraManager_ReleaseDownloaderRights(CAMERA_MOUNT_POSITION);
        if (releaseCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            fprintf(stderr, "Release downloader rights failed: 0x%08lX\n", releaseCode);
            if (returnCode == DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
                returnCode = releaseCode;
            }
        }
    }
    return returnCode;
}

T_DjiReturnCode DjiRpi_ShootSinglePhoto(void)
{
    T_DjiReturnCode returnCode = InitCameraManager();

    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    return ShootSinglePhoto();
}

T_DjiReturnCode DjiRpi_ShootAndDownloadPhoto(const char *outputDirectory)
{
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiReturnCode returnCode = InitCameraManager();

    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }
    returnCode = ShootSinglePhoto();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        return returnCode;
    }

    osalHandler->TaskSleepMs(3000);
    return DownloadLatestPhoto(outputDirectory);
}
