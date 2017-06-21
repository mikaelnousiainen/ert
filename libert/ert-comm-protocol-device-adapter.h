/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_PROTOCOL_DEVICE_ADAPTER_H
#define __ERT_COMM_PROTOCOL_DEVICE_ADAPTER_H

#include "ert-comm-transceiver.h"
#include "ert-comm-protocol.h"

typedef struct _ert_comm_protocol_device_adapter {
  ert_comm_transceiver *comm_transceiver;

  volatile uint32_t transmit_packet_id;

  ert_comm_protocol_device_receive_callback receive_callback;
  void *receive_callback_context;
} ert_comm_protocol_device_adapter;

int ert_comm_protocol_device_adapter_create(ert_comm_transceiver *comm_transceiver,
    ert_comm_protocol_device **comm_protocol_device_rcv);
int ert_comm_protocol_device_adapter_destroy(ert_comm_protocol_device *comm_protocol_device);

#endif
