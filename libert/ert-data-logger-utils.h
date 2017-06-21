/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_UTILS_H
#define __ERT_DATA_LOGGER_UTILS_H

#include "ert-data-logger.h"
#include "ert-log.h"

void ert_data_logger_log_gps_data(ert_log_logger *logger, ert_gps_data *gps_data);
void ert_data_logger_log_comm_device_status(ert_log_logger *logger, ert_comm_device_status *status);
void ert_data_logger_log_sensor_data(ert_log_logger *logger, ert_sensor_module_data *sensor_module_data);
void ert_data_logger_log_entry_root(ert_log_logger *logger, ert_data_logger_entry *entry);
void ert_data_logger_log_entry(ert_log_logger *logger, ert_data_logger_entry *entry);

#endif
