/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_WRITER_ZLOG_H
#define __ERT_DATA_LOGGER_WRITER_ZLOG_H

#include "ert-common.h"
#include "ert-data-logger.h"

int ert_data_logger_writer_zlog_write(ert_data_logger_writer *writer, uint32_t length, uint8_t *data);
int ert_data_logger_writer_zlog_create(const char *category_name, ert_data_logger_writer **writer_rcv);
int ert_data_logger_writer_zlog_destroy(ert_data_logger_writer *writer);

#endif
