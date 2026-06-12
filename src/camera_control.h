#ifndef DJI_RPI_CAMERA_CONTROL_H
#define DJI_RPI_CAMERA_CONTROL_H

#include "dji_typedef.h"

T_DjiReturnCode DjiRpi_ShootSinglePhoto(void);
T_DjiReturnCode DjiRpi_ShootAndDownloadPhoto(const char *outputDirectory);

#endif
