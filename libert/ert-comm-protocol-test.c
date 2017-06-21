/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "ert-comm-protocol-test.h"
#include "ert-log.h"
#include "ert-test.h"

typedef struct _ert_comm_protocol_test_context ert_comm_protocol_test_context;

typedef struct _ert_comm_protocol_test_protocol_context {
  uint32_t device_id;

  ert_comm_protocol *comm_protocol;
  ert_pipe *stream_queue;

  ert_comm_protocol_test_context *comm_protocol_test_context;
} ert_comm_protocol_test_protocol_context;

struct _ert_comm_protocol_test_context {
  ert_comm_transceiver_test_context *comm_transceiver_test_context;

  ert_comm_protocol *comm_protocol1;
  ert_comm_protocol *comm_protocol2;

  ert_comm_protocol_device *comm_protocol_device1;
  ert_comm_protocol_device *comm_protocol_device2;

  ert_comm_protocol_test_protocol_context *comm_protocol1_test_protocol_context;
  ert_comm_protocol_test_protocol_context *comm_protocol2_test_protocol_context;
};

#define STREAM_READ_TIMEOUT_MILLIS 5000

int ert_comm_protocol_test_read_stream(ert_comm_protocol_test_protocol_context *context, ert_comm_protocol_stream *stream)
{
  ert_comm_protocol *comm_protocol = context->comm_protocol;
  ert_comm_protocol_stream_info stream_info;

  uint32_t total_bytes_read = 0;
  uint32_t bytes_read = 0;
  uint32_t buffer_size = 1024;
  uint8_t buffer[buffer_size];
  int result;

  ert_comm_protocol_stream_get_info(stream, &stream_info);

  ert_log_debug("Starting to read data in stream ID %d port %d ...",
      stream_info.stream_id, stream_info.port);

  do {
    ert_log_debug("Reading data in stream ID %d port %d ...", stream_info.stream_id, stream_info.port);
    result = ert_comm_protocol_receive_stream_read(comm_protocol, stream, STREAM_READ_TIMEOUT_MILLIS,
        buffer_size, buffer, &bytes_read);
    if (result == -ETIMEDOUT) {
      ert_log_debug("Read timed out, retrying read for stream ID %d port %d ...", stream_info.stream_id, stream_info.port);
      continue;
    } else if (result < 0) {
      ert_log_error("ert_comm_protocol_receive_stream_read failed with result: %d", result);
      break;
    }

    if (bytes_read > 0) {
      buffer[bytes_read - 1] = '\0';
      total_bytes_read += bytes_read;
      ert_log_debug("Received %d bytes in packet for stream ID %d port %d: '%s'",
          bytes_read, stream_info.stream_id, stream_info.port, buffer);
    }
  } while (bytes_read > 0);

  if (result < 0) {
    ert_comm_protocol_receive_stream_close(comm_protocol, stream);
    return result;
  }

  result = ert_comm_protocol_receive_stream_close(comm_protocol, stream);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_receive_stream_close failed with result: %d", result);
    return result;
  }

  ert_log_debug("Total bytes of data received for stream ID %d port %d: %d",
      stream_info.stream_id, stream_info.port, total_bytes_read);

  return 0;
}


void *ert_comm_protocol_test_stream_reader(void *context)
{
  ert_comm_protocol_test_protocol_context *test_protocol_context = (ert_comm_protocol_test_protocol_context *) context;

  ert_comm_protocol_stream *stream;
  ssize_t pop_result = ert_pipe_pop_timed(test_protocol_context->stream_queue, &stream, 1, 1000);
  if (pop_result < 1) {
    return NULL;
  }

  int result = ert_comm_protocol_test_read_stream(test_protocol_context, stream);
  assert(result == 0);

  return NULL;
}

void ert_comm_protocol_test_stream_listener_callback(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, void *callback_context)
{
  ert_comm_protocol_test_protocol_context *test_protocol_context = (ert_comm_protocol_test_protocol_context *) callback_context;
  ert_comm_protocol_stream_info stream_info;
  int result;

  result = ert_comm_protocol_stream_get_info(stream, &stream_info);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_get_info failed with result %d", result);
    return;
  }

  ert_log_debug("New stream ID %d in port %d", stream_info.stream_id, stream_info.port);

  switch (stream_info.port) {
    case 1:
    case 2:
      ert_pipe_push(test_protocol_context->stream_queue, &stream, 1);
      break;
    default:
      ert_log_error("Unknown port in stream: %d", stream_info.port);
      result = ert_comm_protocol_receive_stream_close(comm_protocol, stream);
      if (result < 0) {
        ert_log_error("ert_comm_protocol_receive_stream_close failed with result %d", result);
      }
      abort();
      return;
  }

  pthread_t thread;
  result = pthread_create(&thread, NULL, ert_comm_protocol_test_stream_reader, callback_context);
  if (result != 0) {
    ert_log_error("Error starting stream reader thread: %s", strerror(errno));
    abort();
    return;
  }
}

int ert_comm_protocol_test_protocol_context_create(ert_comm_protocol_test_context *context,
    uint32_t device_id, ert_comm_protocol_test_protocol_context **protocol_context_rcv)
{
  ert_comm_protocol_test_protocol_context *protocol_context = malloc(sizeof(ert_comm_protocol_test_protocol_context));
  if (protocol_context == NULL) {
    ert_log_fatal("Error allocating memory for test context: %s", strerror(errno));
    return -ENOMEM;
  }

  protocol_context->comm_protocol_test_context = context;
  protocol_context->device_id = device_id;

  int result = ert_pipe_create(sizeof(ert_comm_protocol_stream *), 128, &protocol_context->stream_queue);
  if (result != 0) {
    free(protocol_context);
    ert_log_fatal("Error creating pipe for test protocol context stream queue");
    return -ENOMEM;
  }

  *protocol_context_rcv = protocol_context;

  return 0;
}

int ert_comm_protocol_test_protocol_context_destroy(ert_comm_protocol_test_protocol_context *protocol_context)
{
  ert_pipe_close(protocol_context->stream_queue);
  ert_pipe_destroy(protocol_context->stream_queue);
  free(protocol_context);
  return 0;
}

int ert_comm_protocol_test_initialize(ert_comm_protocol_test_context **context_rcv)
{
  ert_comm_protocol_test_context *context = malloc(sizeof(ert_comm_protocol_test_context));
  if (context == NULL) {
    ert_log_fatal("Error allocating memory for test context: %s", strerror(errno));
    return -ENOMEM;
  }

  int result = ert_comm_transceiver_test_initialize(&context->comm_transceiver_test_context);
  if (result < 0) {
    return result;
  }

  result = ert_comm_protocol_test_protocol_context_create(context, 1, &context->comm_protocol1_test_protocol_context);
  if (result < 0) {
    return result;
  }

  result = ert_comm_protocol_test_protocol_context_create(context, 2, &context->comm_protocol2_test_protocol_context);
  if (result < 0) {
    return result;
  }

  ert_comm_protocol_config config;
  ert_comm_protocol_create_default_config(&config);

  result = ert_comm_protocol_device_adapter_create(context->comm_transceiver_test_context->comm_transceiver1,
      &context->comm_protocol_device1);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_device_adapter_create failed with result: %d", result);
    return result;
  }

  context->comm_protocol1_test_protocol_context->comm_protocol = context->comm_protocol1;

  result = ert_comm_protocol_create(&config, ert_comm_protocol_test_stream_listener_callback,
      context->comm_protocol1_test_protocol_context,
      context->comm_protocol_device1, &context->comm_protocol1);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_create failed with result: %d", result);
    return result;
  }
  context->comm_protocol1_test_protocol_context->comm_protocol = context->comm_protocol1;

  result = ert_comm_protocol_device_adapter_create(context->comm_transceiver_test_context->comm_transceiver2,
      &context->comm_protocol_device2);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_device_adapter_create failed with result: %d", result);
    return result;
  }

  result = ert_comm_protocol_create(&config, ert_comm_protocol_test_stream_listener_callback,
      context->comm_protocol2_test_protocol_context,
      context->comm_protocol_device2, &context->comm_protocol2);
  if (result != 0) {
    ert_log_error("ert_comm_protocol_create failed with result: %d", result);
    return result;
  }
  context->comm_protocol2_test_protocol_context->comm_protocol = context->comm_protocol2;

  *context_rcv = context;

  return 0;
}

int ert_comm_protocol_test_uninitialize(ert_comm_protocol_test_context *context)
{
  ert_comm_protocol_destroy(context->comm_protocol2);
  ert_comm_protocol_destroy(context->comm_protocol1);

  ert_comm_protocol_device_adapter_destroy(context->comm_protocol_device2);
  ert_comm_protocol_device_adapter_destroy(context->comm_protocol_device1);

  ert_comm_protocol_test_protocol_context_destroy(context->comm_protocol2_test_protocol_context);
  ert_comm_protocol_test_protocol_context_destroy(context->comm_protocol1_test_protocol_context);

  ert_comm_transceiver_test_uninitialize(context->comm_transceiver_test_context);

  free(context);

  return 0;
}

int ert_comm_protocol_test_transmit_stream_write(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream, char *data_string)
{
  uint32_t bytes_written = 0;
  uint32_t data_length = (uint32_t) strlen(data_string) + 1;

  int result, retry_count = 3;

  retry_write:
  result = ert_comm_protocol_transmit_stream_write(comm_protocol, stream,
      data_length, (uint8_t *) data_string, &bytes_written);

  if (result == -EAGAIN && retry_count > 0) {
    retry_count--;
    sleep(1);
    goto retry_write;
  } else if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_write failed with result %d", result);
    return result;
  }

  assert(bytes_written == data_length);

  return result;
}

int ert_comm_protocol_test_transmit_stream_flush(ert_comm_protocol *comm_protocol,
    ert_comm_protocol_stream *stream)
{
  int result, retry_count = 3;

  retry_flush:
  result = ert_comm_protocol_transmit_stream_flush(comm_protocol, stream, false, NULL);
  if (result == -EAGAIN && retry_count > 0) {
    retry_count--;
    sleep(1);
    goto retry_flush;
  } else if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_flush failed with result %d", result);
    return result;
  }

  return result;
}

void ert_comm_protocol_test_assert_stream_info_no_errors(ert_comm_protocol_stream *stream)
{
  ert_comm_protocol_stream_info stream_info;

  int result = ert_comm_protocol_stream_get_info(stream, &stream_info);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_stream_get_info failed with result %d", result);
  }
  assert(result == 0);

  assert(!stream_info.failed);

  assert(stream_info.acks_enabled);
  assert(!stream_info.acks);
  assert(stream_info.ack_rerequest_count == 0);
  assert(stream_info.end_of_stream_ack_rerequest_count == 0);

  assert(stream_info.received_packet_sequence_number_error_count == 0);

  assert(stream_info.duplicate_transferred_packet_count == 0);

  assert(stream_info.retransmitted_packet_count == 0);
  assert(stream_info.retransmitted_data_bytes == 0);
  assert(stream_info.retransmitted_payload_data_bytes == 0);
}

void ert_comm_protocol_test_run_test_full_packets_less_than_ack_interval(ert_comm_protocol_test_context *context)
{
  ert_comm_protocol_stream *stream1;

  int result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 1, &stream1,
      ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
  }
  assert(result == 0);

  int write_count = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT;

  ert_log_info("Writing %d times with acks ...", write_count);

  for (int i = 0; i < write_count; i++) {
    char data[255];
    snprintf(data, 255, "Write %d:data1data2data3data4:", i);

    result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream1, data);
    assert(result == 0);
  }

  result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
  assert(result == 0);

  ert_comm_protocol_test_assert_stream_info_no_errors(stream1);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream1, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);
}

void ert_comm_protocol_test_run_test_short_packets_more_than_ack_interval(ert_comm_protocol_test_context *context)
{
  ert_comm_protocol_stream *stream1;

  int result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 1, &stream1,
      ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
  }
  assert(result == 0);

  int packet_count = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT +
      ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT / 2;

  ert_log_info("Transmitting %d packets with acks ...", packet_count);

  for (int i = 0; i < packet_count; i++) {
    char data[255];
    snprintf(data, 255, "Packet %d", i);

    result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream1, data);
    assert(result == 0);

    result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
    assert(result == 0);
  }

  ert_comm_protocol_test_assert_stream_info_no_errors(stream1);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream1, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);
}

void ert_comm_protocol_test_run_test_two_streams_more_than_ack_interval(ert_comm_protocol_test_context *context)
{
  ert_comm_protocol_stream *stream1;
  ert_comm_protocol_stream *stream2;
  int result;

  result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 1, &stream1,
      ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
  }
  assert(result == 0);
  result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 2, &stream2,
      ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
  }
  assert(result == 0);

  int loop_count = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT +
      ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT / 2;

  ert_log_info("Transmitting %d packets with acks in stream 1 and %d writes for full packets in stream 2...", loop_count / 4, loop_count);

  for (int i = 0; i < loop_count; i++) {
    char data[255];

    snprintf(data, 255, "Packet %d", i);
    result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream1, data);
    assert(result == 0);

    snprintf(data, 255, "Write %d:dummydata1-dummydata2:", i);
    result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream2, data);
    assert(result == 0);

    if (i % 4 == 0) {
      result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
      assert(result == 0);
    }
  }

  result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
  assert(result == 0);
  result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream2);
  assert(result == 0);

  ert_comm_protocol_test_assert_stream_info_no_errors(stream1);
  ert_comm_protocol_test_assert_stream_info_no_errors(stream2);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream1, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream2, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);
}

void ert_comm_protocol_test_run_test_multiple_streams_over_one_more_than_ack_interval(ert_comm_protocol_test_context *context)
{
  ert_comm_protocol_stream *stream1 = NULL;
  ert_comm_protocol_stream *stream2 = NULL;
  int result;

  result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 1, &stream1,
      ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
  }
  assert(result == 0);

  int loop_count = ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT * 28 +
      ERT_COMM_PROTOCOL_STREAM_ACK_INTERVAL_PACKET_COUNT_DEFAULT / 2;

  ert_log_info("Transmitting %d packets with acks in stream 1 and %d writes for full packets over multiple streams in stream 2...", loop_count / 4, loop_count);

  for (int i = 0; i < loop_count; i++) {
    char data[255];

    snprintf(data, 255, "Packet %d", i);
    result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream1, data);
    assert(result == 0);

    if (stream2 != NULL) {
      snprintf(data, 255, "Write %d:dummydata1-dummydata2:", i);
      result = ert_comm_protocol_test_transmit_stream_write(context->comm_protocol1, stream2, data);
      assert(result == 0);
    }

    if (i % 4 == 0) {
      result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
      assert(result == 0);
    }

    if (i % 7 == 0) {
      if (stream2 != NULL) {
        result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream2);
        assert(result == 0);

        ert_comm_protocol_test_assert_stream_info_no_errors(stream2);

        result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream2, false);
        if (result < 0) {
          ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
        }
        assert(result == 0);
      }

      result = ert_comm_protocol_transmit_stream_open(context->comm_protocol1, 2, &stream2,
          ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED);
      if (result < 0) {
        ert_log_error("ert_comm_protocol_transmit_stream_open failed with result %d", result);
      }
      assert(result == 0);
    }
  }

  result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream1);
  assert(result == 0);
  result = ert_comm_protocol_test_transmit_stream_flush(context->comm_protocol1, stream2);
  assert(result == 0);

  ert_comm_protocol_test_assert_stream_info_no_errors(stream1);
  ert_comm_protocol_test_assert_stream_info_no_errors(stream2);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream1, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);

  result = ert_comm_protocol_transmit_stream_close(context->comm_protocol1, stream2, false);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result %d", result);
  }
  assert(result == 0);
}

int main(void)
{
  int result = ert_test_init();
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_comm_protocol_test_context *context;
  result = ert_comm_protocol_test_initialize(&context);
  if (result < 0) {
    return EXIT_FAILURE;
  }

  ert_comm_protocol_test_run_test_full_packets_less_than_ack_interval(context);

  ert_comm_protocol_test_run_test_short_packets_more_than_ack_interval(context);

  ert_comm_protocol_test_run_test_two_streams_more_than_ack_interval(context);

  ert_comm_protocol_test_run_test_multiple_streams_over_one_more_than_ack_interval(context);

  ert_log_info("Tests finished successfully");

  sleep(5);

  ert_comm_protocol_test_uninitialize(context);

  ert_test_uninit();

  return EXIT_SUCCESS;
}
