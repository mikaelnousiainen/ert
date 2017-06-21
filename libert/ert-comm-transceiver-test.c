/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "ert-comm-transceiver-test.h"
#include "ert-log.h"
#include "ert-test.h"

int ert_comm_transceiver_test_run_test_basic(ert_comm_transceiver_test_context *context)
{
  char *expected_packet_data_device1[] = {
      "Device 2: Packet 1",
      "Device 2: Packet 2",
      "Device 2: Packet 5 - no transmit callback",
      "Device 2: Packet 6 - normal",
      NULL,
  };
  char *expected_packet_data_device2[] = {
      "Device 1: Packet 1",
      "Device 1: Packet 2",
      NULL,
  };

  int result = ert_comm_transceiver_test_transmit(context->comm_transceiver1, 11, "Device 1: Packet 1");
  assert(result == 0);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 21, "Device 2: Packet 1");
  assert(result == 0);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver1, 12, "Device 1: Packet 2");
  assert(result == 0);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 22, "Device 2: Packet 2");
  assert(result == 0);

  ert_log_info("Make transmission of packet fail");
  ert_driver_comm_device_dummy_set_fail_transmit(context->device2, true);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 27, "Device 2: Packet 7 - fail transmit");
  assert(result == -EIO);
  ert_driver_comm_device_dummy_set_fail_transmit(context->device2, false);

  ert_log_info("Make reception of packet fail");
  ert_driver_comm_device_dummy_set_fail_receive(context->device1, true);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 23, "Device 2: Packet 3 - fail receive");
  assert(result == 0);
  ert_driver_comm_device_dummy_set_fail_receive(context->device1, false);

  ert_log_info("Force packet loss");
  ert_driver_comm_device_dummy_set_lose_packets(context->device2, true);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 24, "Device 2: Packet 4 - lose packets");
  assert(result == 0);
  ert_driver_comm_device_dummy_set_lose_packets(context->device2, false);

  ert_log_info("Do not invoke transmit callback -> forces transmit to timeout");
  ert_driver_comm_device_dummy_set_no_transmit_callback(context->device2, true);
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 25, "Device 2: Packet 5 - no transmit callback");
  assert(result == -ETIMEDOUT);
  ert_driver_comm_device_dummy_set_no_transmit_callback(context->device2, false);

  ert_log_info("Normal transmit");
  result = ert_comm_transceiver_test_transmit(context->comm_transceiver2, 26, "Device 2: Packet 6 - normal");
  assert(result == 0);

  result = ert_comm_transceiver_test_verify_received_packets(context->device_context1, expected_packet_data_device1);
  assert(result == 0);
  result = ert_comm_transceiver_test_verify_received_packets(context->device_context2, expected_packet_data_device2);
  assert(result == 0);

  return 0;
}

int main(void)
{
  int result = ert_test_init();
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_comm_transceiver_test_context *context;
  result = ert_comm_transceiver_test_initialize(&context);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_comm_transceiver_test_run_test_basic(context);

  ert_log_info("Tests finished successfully");

  sleep(2);

  ert_comm_transceiver_test_uninitialize(context);

  ert_test_uninit();

  return EXIT_SUCCESS;
}