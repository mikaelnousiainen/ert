/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_SERIALIZER_JANSSON_H
#define __ERT_DATA_LOGGER_SERIALIZER_JANSSON_H

#include "ert-common.h"
#include "ert-data-logger.h"

int ert_data_logger_serializer_jansson_serialize(ert_data_logger_serializer *serializer,
    ert_data_logger_entry *entry, uint32_t *length, uint8_t **data_rcv);
int ert_data_logger_serializer_jansson_create(ert_data_logger_serializer **serializer_rcv);
int ert_data_logger_serializer_jansson_destroy(ert_data_logger_serializer *serializer);

#endif
