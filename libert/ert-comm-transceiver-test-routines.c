/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <memory.h>
#include <errno.h>

#include "ert-comm-transceiver-test-routines.h"
#include "ert-log.h"

static void comm_transceiver_receive_callback(uint32_t length, uint8_t *data, void *callback_context)
{
  ert_comm_transceiver_test_device_context *device_context =
      (ert_comm_transceiver_test_device_context *) callback_context;

  ert_log_info("Received packet of %d bytes at device %d: '%s'", length, device_context->device_id, data);

  ert_comm_transceiver_test_packet packet;
  packet.data_length = length;
  memcpy(packet.data, data, length);

  ert_pipe_push(device_context->received_packets_queue, &packet, 1);
}

int ert_comm_transceiver_test_context_create(ert_comm_transceiver_test_context **context_rcv)
{
  ert_comm_transceiver_test_context *context = malloc(sizeof(ert_comm_transceiver_test_context));
  if (context == NULL) {
    ert_log_fatal("Error allocating memory for test context: %s", strerror(errno));
    return -ENOMEM;
  }

  *context_rcv = context;

  return 0;
}

int ert_comm_transceiver_test_context_destroy(ert_comm_transceiver_test_context *context)
{
  free(context);
  return 0;
}

int ert_comm_transceiver_test_device_context_create(ert_comm_transceiver_test_context *context,
    uint32_t device_id, ert_comm_transceiver_test_device_context **device_context_rcv)
{
  ert_comm_transceiver_test_device_context *device_context = malloc(sizeof(ert_comm_transceiver_test_device_context));
  if (device_context == NULL) {
    ert_log_fatal("Error allocating memory for test context: %s", strerror(errno));
    return -ENOMEM;
  }

  device_context->test_context = context;
  device_context->device_id = device_id;

  int result = ert_pipe_create(sizeof(ert_comm_transceiver_test_packet), 128, &device_context->received_packets_queue);
  if (result != 0) {
    free(device_context);
    ert_log_fatal("Error creating pipe for test device context packets queue");
    return -ENOMEM;
  }

  *device_context_rcv = device_context;

  return 0;
}

int ert_comm_transceiver_test_device_context_destroy(ert_comm_transceiver_test_device_context *device_context)
{
  ert_pipe_close(device_context->received_packets_queue);
  ert_pipe_destroy(device_context->received_packets_queue);
  free(device_context);
  return 0;
}

int ert_comm_transceiver_test_transmit(ert_comm_transceiver *comm_transceiver, uint32_t id, char *string_data)
{
  uint32_t data_length = (uint32_t) (strlen(string_data) + 1);

  uint32_t bytes_transmitted;
  int result = ert_comm_transceiver_transmit(comm_transceiver, id, data_length, (uint8_t *) string_data,
      ERT_COMM_TRANSCEIVER_TRANSMIT_FLAG_BLOCK, &bytes_transmitted);
  if (result < 0) {
    ert_log_error("ert_comm_transceiver_transmit failed with result %d", result);
  } else {
    if (bytes_transmitted != data_length) {
      ert_log_error("ert_comm_transceiver_transmit: bytes_transmitted != data_length");
    }
  }

  return result;
}

int ert_comm_transceiver_test_verify_received_packets(ert_comm_transceiver_test_device_context *device_context, char *packet_data_array[])
{
  size_t index = 0;
  while (packet_data_array[index] != NULL ) {
    char *packet_data = packet_data_array[index];
    ert_log_info("Verifying received packet for device %d: '%s'", device_context->device_id, packet_data);

    ert_comm_transceiver_test_packet packet;
    ssize_t pop_result = ert_pipe_pop_timed(device_context->received_packets_queue, &packet, 1, 1);
    if (pop_result < 0) {
      ert_log_error("Not enough received packets in queue");
      return -EINVAL;
    }
    if (pop_result == 0) {
      ert_log_error("BUG: Received packets pipe was closed");
      return -EINVAL;
    }

    if (strcmp(packet_data, (char *) packet.data) != 0) {
      ert_log_error("Packet data does not match: found '%s', expected '%s'", packet.data, packet_data);
      return -EINVAL;
    }
    uint32_t expected_data_length = (uint32_t) strlen(packet_data) + 1;
    if (packet.data_length != expected_data_length) {
      ert_log_error("Packet data length does not match: found %d, expected %d", packet.data_length, expected_data_length);
      return -EINVAL;
    }

    ert_log_info("Verifying received packet for device %d: '%s' - OK", device_context->device_id, packet_data);

    index++;
  }

  ert_comm_transceiver_test_packet packet;
  ssize_t pop_result = ert_pipe_pop_timed(device_context->received_packets_queue, &packet, 1, 1);
  if (pop_result >= 0) {
    ert_log_error("Received packets queue contains more entries than expected");
    return -EINVAL;
  }

  return 0;
}

int ert_comm_transceiver_test_initialize(ert_comm_transceiver_test_context **context_rcv)
{
  ert_comm_transceiver_test_context *context;
  int result = ert_comm_transceiver_test_context_create(&context);
  if (result < 0) {
    ert_log_error("Error creating test context");
    return -EIO;
  }

  ert_comm_driver_dummy_config comm_driver_dummy_config1 = {0};
  comm_driver_dummy_config1.transmit_time_millis = ERT_TRANSCEIVER_TEST_TRANSMIT_TIME_MILLIS;
  comm_driver_dummy_config1.max_packet_length = ERT_TRANSCEIVER_TEST_MAX_PACKET_LENGTH;

  result = ert_driver_comm_device_dummy_open(&comm_driver_dummy_config1, &context->device1);
  if (result < 0) {
    ert_log_error("Error opening dummy device 1");
    return -EIO;
  }

  ert_comm_driver_dummy_config comm_driver_dummy_config2 = {0};
  comm_driver_dummy_config2.transmit_time_millis = ERT_TRANSCEIVER_TEST_TRANSMIT_TIME_MILLIS;
  comm_driver_dummy_config2.max_packet_length = ERT_TRANSCEIVER_TEST_MAX_PACKET_LENGTH;

  result = ert_driver_comm_device_dummy_open(&comm_driver_dummy_config2, &context->device2);
  if (result < 0) {
    ert_log_error("Error opening dummy device 2");
    return -EIO;
  }

  ert_driver_comm_device_dummy_connect(context->device1, context->device2);

  result = ert_comm_transceiver_test_device_context_create(context, 1, &context->device_context1);
  if (result < 0) {
    ert_log_error("Error creating test device context 1");
    return -EIO;
  }

  result = ert_comm_transceiver_test_device_context_create(context, 2, &context->device_context2);
  if (result < 0) {
    ert_log_error("Error creating test device context 2");
    return -EIO;
  }

  ert_comm_transceiver_config comm_transceiver_config1 = {0};
  comm_transceiver_config1.transmit_buffer_length_packets = 32;
  comm_transceiver_config1.receive_buffer_length_packets = 32;
  comm_transceiver_config1.transmit_timeout_milliseconds = 10000;
  comm_transceiver_config1.poll_interval_milliseconds = 1000;
  comm_transceiver_config1.receive_callback = comm_transceiver_receive_callback;
  comm_transceiver_config1.receive_callback_context = context->device_context1;

  result = ert_comm_transceiver_start(context->device1, &comm_transceiver_config1, &context->comm_transceiver1);
  if (result < 0) {
    ert_log_error("Error starting comm transceiver 1");
    return -EIO;
  }

  ert_comm_transceiver_config comm_transceiver_config2 = {0};
  comm_transceiver_config2.transmit_buffer_length_packets = 32;
  comm_transceiver_config2.receive_buffer_length_packets = 32;
  comm_transceiver_config2.transmit_timeout_milliseconds = 10000;
  comm_transceiver_config2.poll_interval_milliseconds = 1000;
  comm_transceiver_config2.receive_callback = comm_transceiver_receive_callback;
  comm_transceiver_config2.receive_callback_context = context->device_context2;

  result = ert_comm_transceiver_start(context->device2, &comm_transceiver_config2, &context->comm_transceiver2);
  if (result < 0) {
    ert_log_error("Error starting comm transceiver 2");
    return -EIO;
  }

  *context_rcv = context;

  return 0;
}

int ert_comm_transceiver_test_uninitialize(ert_comm_transceiver_test_context *context)
{
  ert_comm_transceiver_stop(context->comm_transceiver2);
  ert_comm_transceiver_stop(context->comm_transceiver1);

  ert_comm_transceiver_test_device_context_destroy(context->device_context2);
  ert_comm_transceiver_test_device_context_destroy(context->device_context1);

  ert_driver_comm_device_dummy_close(context->device2);
  ert_driver_comm_device_dummy_close(context->device1);

  ert_comm_transceiver_test_context_destroy(context);

  return 0;
}
