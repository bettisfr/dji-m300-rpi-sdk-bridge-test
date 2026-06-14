#include "mop_server.h"

#include "dji_mop_channel.h"
#include "dji_platform.h"
#include "gimbal_control.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOP_COMMAND_CHANNEL_ID 49152
#define MOP_BUFFER_SIZE 4096
#define INFERENCE_RESULT_SIZE 1024

typedef enum {
    INFERENCE_IDLE,
    INFERENCE_RUNNING,
    INFERENCE_COMPLETE,
    INFERENCE_FAILED
} E_InferenceState;

static pthread_mutex_t s_inferenceMutex = PTHREAD_MUTEX_INITIALIZER;
static E_InferenceState s_inferenceState = INFERENCE_IDLE;
static char s_inferenceResult[INFERENCE_RESULT_SIZE] = "STATUS IDLE";

static T_DjiReturnCode SendResponse(
    T_DjiMopChannelHandle channel,
    const char *response);

static void SetInferenceResult(E_InferenceState state, const char *result)
{
    pthread_mutex_lock(&s_inferenceMutex);
    s_inferenceState = state;
    snprintf(s_inferenceResult, sizeof(s_inferenceResult), "%s", result);
    pthread_mutex_unlock(&s_inferenceMutex);
}

static void *InferenceTask(void *arg)
{
    const char *root = getenv("DJI_RPI_ROOT");
    const char *python = getenv("DJI_RPI_PYTHON");
    char command[2048];
    char line[INFERENCE_RESULT_SIZE];
    char result[INFERENCE_RESULT_SIZE] = "";
    FILE *process;
    int status;

    (void) arg;
    if (root == NULL || python == NULL) {
        SetInferenceResult(
            INFERENCE_FAILED,
            "RESULT ERROR missing DJI_RPI_ROOT or DJI_RPI_PYTHON");
        return NULL;
    }

    snprintf(
        command,
        sizeof(command),
        "\"%s\" \"%s/scripts/infer_latest.py\" --root \"%s\" 2>&1",
        python,
        root,
        root);
    process = popen(command, "r");
    if (process == NULL) {
        SetInferenceResult(INFERENCE_FAILED, "RESULT ERROR unable to start Python");
        return NULL;
    }

    while (fgets(line, sizeof(line), process) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strncmp(line, "RESULT ", 7) == 0) {
            snprintf(result, sizeof(result), "%s", line);
        } else if (line[0] != '\0') {
            fprintf(stderr, "Inference: %s\n", line);
        }
    }
    status = pclose(process);

    if (status == 0 && result[0] != '\0') {
        SetInferenceResult(INFERENCE_COMPLETE, result);
    } else {
        snprintf(
            result,
            sizeof(result),
            "RESULT ERROR Python exited with status %d",
            status);
        SetInferenceResult(INFERENCE_FAILED, result);
    }
    return NULL;
}

static int StartInference(void)
{
    pthread_t thread;
    int createResult;

    pthread_mutex_lock(&s_inferenceMutex);
    if (s_inferenceState == INFERENCE_RUNNING) {
        pthread_mutex_unlock(&s_inferenceMutex);
        return 0;
    }
    s_inferenceState = INFERENCE_RUNNING;
    snprintf(s_inferenceResult, sizeof(s_inferenceResult), "STATUS RUNNING");
    pthread_mutex_unlock(&s_inferenceMutex);

    createResult = pthread_create(&thread, NULL, InferenceTask, NULL);
    if (createResult != 0) {
        SetInferenceResult(INFERENCE_FAILED, "RESULT ERROR unable to create worker");
        return 0;
    }
    pthread_detach(thread);
    return 1;
}

static T_DjiReturnCode SendInferenceStatus(T_DjiMopChannelHandle channel)
{
    char response[INFERENCE_RESULT_SIZE + 2];

    pthread_mutex_lock(&s_inferenceMutex);
    snprintf(response, sizeof(response), "%s\n", s_inferenceResult);
    pthread_mutex_unlock(&s_inferenceMutex);
    return SendResponse(channel, response);
}

static T_DjiReturnCode SendResponse(
    T_DjiMopChannelHandle channel,
    const char *response)
{
    uint32_t realLength = 0;
    T_DjiReturnCode returnCode = DjiMopChannel_SendData(
        channel,
        (uint8_t *) response,
        (uint32_t) strlen(response),
        &realLength);

    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "MOP send failed: 0x%08lX\n", returnCode);
        return returnCode;
    }
    if (realLength != strlen(response)) {
        fprintf(stderr, "MOP sent only %u of %zu bytes\n", realLength, strlen(response));
        return DJI_ERROR_SYSTEM_MODULE_CODE_SYSTEM_ERROR;
    }
    return DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS;
}

static T_DjiReturnCode HandleCommand(
    T_DjiMopChannelHandle channel,
    const char *command)
{
    T_DjiReturnCode returnCode;

    if (strcmp(command, "PING") == 0) {
        return SendResponse(channel, "PONG\n");
    }
    if (strcmp(command, "INFER") == 0) {
        if (!StartInference()) {
            return SendResponse(channel, "BUSY INFER\n");
        }
        return SendResponse(channel, "ACCEPTED INFER\n");
    }
    if (strcmp(command, "INFER_STATUS") == 0) {
        return SendInferenceStatus(channel);
    }
    if (strcmp(command, "GIMBAL_YAW -10") == 0) {
        returnCode = SendResponse(channel, "ACCEPTED GIMBAL_YAW -10\n");
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            return returnCode;
        }
        returnCode = DjiRpi_MoveGimbalRelative(0.0f, -10.0f);
        return returnCode;
    }
    if (strcmp(command, "GIMBAL_YAW 10") == 0) {
        returnCode = SendResponse(channel, "ACCEPTED GIMBAL_YAW 10\n");
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            return returnCode;
        }
        returnCode = DjiRpi_MoveGimbalRelative(0.0f, 10.0f);
        return returnCode;
    }
    if (strcmp(command, "GIMBAL_PITCH -10") == 0) {
        returnCode = SendResponse(channel, "ACCEPTED GIMBAL_PITCH -10\n");
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            return returnCode;
        }
        returnCode = DjiRpi_MoveGimbalRelative(-10.0f, 0.0f);
        return returnCode;
    }
    if (strcmp(command, "GIMBAL_PITCH 10") == 0) {
        returnCode = SendResponse(channel, "ACCEPTED GIMBAL_PITCH 10\n");
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            return returnCode;
        }
        returnCode = DjiRpi_MoveGimbalRelative(10.0f, 0.0f);
        return returnCode;
    }
    return SendResponse(channel, "ERROR UNKNOWN_COMMAND\n");
}

T_DjiReturnCode DjiRpi_RunMopServer(void)
{
    T_DjiMopChannelHandle server = NULL;
    T_DjiOsalHandler *osalHandler = DjiPlatform_GetOsalHandler();
    T_DjiReturnCode returnCode;

    returnCode = DjiMopChannel_Init();
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "MOP initialization failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    returnCode = DjiMopChannel_Create(
        &server,
        DJI_MOP_CHANNEL_TRANS_RELIABLE);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "MOP channel creation failed: 0x%08lX\n", returnCode);
        return returnCode;
    }

    returnCode = DjiMopChannel_Bind(server, MOP_COMMAND_CHANNEL_ID);
    if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
        fprintf(stderr, "MOP channel bind failed: 0x%08lX\n", returnCode);
        DjiMopChannel_Destroy(server);
        return returnCode;
    }

    printf(
        "MOP command server listening on reliable channel %u\n",
        MOP_COMMAND_CHANNEL_ID);
    fflush(stdout);

    while (1) {
        T_DjiMopChannelHandle client = NULL;

        returnCode = DjiMopChannel_Accept(server, &client);
        if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
            fprintf(stderr, "MOP accept failed: 0x%08lX\n", returnCode);
            osalHandler->TaskSleepMs(1000);
            continue;
        }

        printf("MOP client connected\n");
        fflush(stdout);

        while (1) {
            uint8_t buffer[MOP_BUFFER_SIZE];
            uint32_t realLength = 0;
            char *lineEnd;

            memset(buffer, 0, sizeof(buffer));
            returnCode = DjiMopChannel_RecvData(
                client,
                buffer,
                sizeof(buffer) - 1,
                &realLength);
            if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
                if (returnCode ==
                    DJI_ERROR_MOP_CHANNEL_MODULE_CODE_CONNECTION_CLOSE) {
                    printf("MOP client disconnected\n");
                    break;
                }
                continue;
            }

            buffer[realLength] = '\0';
            lineEnd = strpbrk((char *) buffer, "\r\n");
            if (lineEnd != NULL) {
                *lineEnd = '\0';
            }

            printf("MOP received: %s\n", buffer);
            fflush(stdout);
            returnCode = HandleCommand(client, (char *) buffer);
            if (returnCode != DJI_ERROR_SYSTEM_MODULE_CODE_SUCCESS) {
                fprintf(stderr, "MOP command failed: 0x%08lX\n", returnCode);
            }
        }

        DjiMopChannel_Close(client);
        DjiMopChannel_Destroy(client);
    }
}
