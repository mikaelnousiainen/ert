/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_PROTOCOL_JSON_H
#define __ERT_COMM_PROTOCOL_JSON_H

#include "ert-comm-protocol.h"

int ert_comm_protocol_stream_info_json_serialize(size_t stream_info_count, ert_comm_protocol_stream_info *stream_info,
    uint32_t *length, uint8_t **data_rcv);

#endif
