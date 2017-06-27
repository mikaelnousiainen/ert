#ifndef __ERT_GPS_UBLOX_H
#define __ERT_GPS_UBLOX_H

#include "ert-common.h"
#include "ert-hal-serial.h"

int ert_gps_ublox_configure_navigation_engine(hal_serial_device *serial_device);
int ert_gps_ublox_configure(hal_serial_device_config *serial_device_config);

#endif
