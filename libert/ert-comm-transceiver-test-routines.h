/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_COMM_TRANSCEIVER_TEST_ROUTINES_H
#define __ERT_COMM_TRANSCEIVER_TEST_ROUTINES_H

#include "ert-common.h"
#include "ert-comm-transceiver.h"
#include "ert-comm-device-dummy.h"

#define ERT_TRANSCEIVER_TEST_TRANSMIT_TIME_MILLIS 0
#define ERT_TRANSCEIVER_TEST_MAX_PACKET_LENGTH 255

typedef struct _ert_comm_transceiver_test_packet {
  uint32_t data_length;
  uint8_t data[ERT_TRANSCEIVER_TEST_MAX_PACKET_LENGTH];
} ert_comm_transceiver_test_packet;

typedef struct _ert_comm_transceiver_test_context ert_comm_transceiver_test_context;

typedef struct _ert_comm_transceiver_test_device_context {
  uint32_t device_id;
  ert_comm_transceiver_test_context *test_context;
  ert_pipe *received_packets_queue;
} ert_comm_transceiver_test_device_context;

struct _ert_comm_transceiver_test_context {
  ert_comm_device *device1;
  ert_comm_device *device2;

  ert_comm_transceiver *comm_transceiver1;
  ert_comm_transceiver *comm_transceiver2;

  ert_comm_transceiver_test_device_context *device_context1;
  ert_comm_transceiver_test_device_context *device_context2;
};

int ert_comm_transceiver_test_context_create(ert_comm_transceiver_test_context **context_rcv);
int ert_comm_transceiver_test_context_destroy(ert_comm_transceiver_test_context *context);
int ert_comm_transceiver_test_device_context_create(ert_comm_transceiver_test_context *context,
    uint32_t device_id, ert_comm_transceiver_test_device_context **device_context_rcv);
int ert_comm_transceiver_test_device_context_destroy(ert_comm_transceiver_test_device_context *device_context);
int ert_comm_transceiver_test_transmit(ert_comm_transceiver *comm_transceiver, uint32_t id, char *string_data);
int ert_comm_transceiver_test_verify_received_packets(ert_comm_transceiver_test_device_context *device_context, char *packet_data_array[]);
int ert_comm_transceiver_test_initialize(ert_comm_transceiver_test_context **context_rcv);
int ert_comm_transceiver_test_uninitialize(ert_comm_transceiver_test_context *context);

#endif
