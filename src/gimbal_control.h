#ifndef DJI_RPI_GIMBAL_CONTROL_H
#define DJI_RPI_GIMBAL_CONTROL_H

#include "dji_typedef.h"

T_DjiReturnCode DjiRpi_SetGimbalPitch(float targetPitchDegrees);
T_DjiReturnCode DjiRpi_MoveGimbalRelative(
    float pitchDegrees,
    float yawDegrees);
T_DjiReturnCode DjiRpi_RunGimbalConsole(void);

#endif
