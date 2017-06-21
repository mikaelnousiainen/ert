/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_PROTOCOL_HELPERS_H
#define __ERT_COMM_PROTOCOL_HELPERS_H

#include "ert-comm-protocol.h"

int ert_comm_protocol_transmit_buffer(ert_comm_protocol *comm_protocol, uint8_t port,
    bool enable_acks, uint32_t data_length, uint8_t *data);
int ert_comm_protocol_transmit_file_and_buffer(ert_comm_protocol *comm_protocol, uint8_t port,
    bool enable_acks, const char *filename, uint32_t data_length, uint8_t *data, volatile bool *running);
int ert_comm_protocol_transmit_file(ert_comm_protocol *comm_protocol, uint8_t port,
    bool enable_acks, const char *filename, volatile bool *running);

int ert_comm_protocol_receive_buffer(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t buffer_size, uint8_t *buffer, uint32_t *bytes_received, volatile bool *running);
int ert_comm_protocol_receive_file(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream, const char *filename,
    bool delete_empty, volatile bool *running, uint32_t *bytes_received_rcv);

#endif
