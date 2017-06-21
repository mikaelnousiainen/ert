/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "ert-comm-protocol-helpers.h"
#include "ert-log.h"

#define FILE_TRANSMIT_BUFFER_LENGTH 1024
#define TRANSMIT_RETRY_COUNT 3
#define TRANSMIT_RETRY_DELAY_MILLIS 2000

#define STREAM_READ_TIMEOUT_MILLIS 5000
#define STREAM_FILE_DATA_BUFFER_LENGTH (16 * 1024)

static int ert_comm_protocol_write_buffer(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t data_length, uint8_t *data, uint32_t *bytes_written)
{
  uint16_t retry_count = 0;
  int result;

  retry_write:
  result = ert_comm_protocol_transmit_stream_write(comm_protocol, stream, data_length, data, bytes_written);
  if (result == -EAGAIN && retry_count < TRANSMIT_RETRY_COUNT) {
    retry_count++;
    ert_log_info("Retrying write: %d of %d", retry_count, TRANSMIT_RETRY_COUNT);
    usleep(TRANSMIT_RETRY_DELAY_MILLIS * 1000);
    goto retry_write;
  } else if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_write failed with result: %d", result);
    return result;
  }

  return 0;
}

int ert_comm_protocol_transmit_buffer(ert_comm_protocol *comm_protocol, uint8_t port, bool enable_acks, uint32_t data_length, uint8_t *data)
{
  int result, close_result;
  uint16_t retry_count;
  ert_comm_protocol_stream *stream;

  result = ert_comm_protocol_transmit_stream_open(comm_protocol, port, &stream,
      enable_acks ? ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED : 0);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result: %d", result);
    return result;
  }

  uint32_t bytes_written = 0;
  result = ert_comm_protocol_write_buffer(comm_protocol, stream, data_length, data, &bytes_written);
  if (result < 0) {
    goto error_close_stream;
  }

  retry_count = 0;

  retry_close:
  result = ert_comm_protocol_transmit_stream_close(comm_protocol, stream, false);
  if (result == -EAGAIN && retry_count < TRANSMIT_RETRY_COUNT) {
    retry_count++;
    ert_log_info("Retrying close: %d of %d", retry_count, TRANSMIT_RETRY_COUNT);
    usleep(TRANSMIT_RETRY_DELAY_MILLIS * 1000);
    goto retry_close;
  } else if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result: %d", result);
    goto error_close_stream;
  }

  return bytes_written;

  error_close_stream:
  close_result = ert_comm_protocol_transmit_stream_close(comm_protocol, stream, true);
  if (close_result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result: %d", close_result);
    return close_result;
  }

  return result;
}

int ert_comm_protocol_transmit_file_and_buffer(ert_comm_protocol *comm_protocol, uint8_t port,
    bool enable_acks, const char *filename, uint32_t data_length, uint8_t *data, volatile bool *running)
{
  size_t buffer_length = FILE_TRANSMIT_BUFFER_LENGTH;
  uint8_t buffer[buffer_length];
  uint16_t retry_count;
  int result, close_result;

  struct stat st;
  result = stat(filename, &st);
  if (result < 0) {
    ert_log_error("Error checking file '%s' status: %s", strerror(errno));
    return result;
  }
  uint32_t filesize = (uint32_t) st.st_size;

  ert_comm_protocol_stream *stream;
  result = ert_comm_protocol_transmit_stream_open(comm_protocol, port, &stream,
      enable_acks ? ERT_COMM_PROTOCOL_STREAM_FLAG_ACKS_ENABLED : 0);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_open failed with result: %d", result);
    return result;
  }

  ert_log_info("Transmitting file '%s' with size of %d bytes ...", filename, filesize);

  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    ert_log_error("Error opening file '%s' for reading: %s", filename, strerror(errno));
    goto error_close_stream;
  }

  ssize_t read_result;
  uint32_t offset = 0;
  do {
    ert_log_debug("Reading at %d/%d of file '%s' ...", offset, filesize, filename);

    read_result = read(fd, buffer, buffer_length);
    if (read_result < 0) {
      ert_log_error("Error reading file '%s': %s", filename, strerror(errno));
      goto error_close_file;
    }

    uint32_t read_bytes = (uint32_t) read_result;

    ert_log_debug("Transmitting %d bytes at %d/%d of file '%s' ...", read_bytes, offset, filesize, filename);

    uint32_t bytes_written = 0;
    retry_count = 0;

    retry_write:
    result = ert_comm_protocol_transmit_stream_write(comm_protocol, stream,
        read_bytes, buffer, &bytes_written);
    if (result == -EAGAIN && retry_count < TRANSMIT_RETRY_COUNT) {
      retry_count++;
      ert_log_info("Retrying write: %d of %d", retry_count, TRANSMIT_RETRY_COUNT);
      usleep(TRANSMIT_RETRY_DELAY_MILLIS * 1000);
      goto retry_write;
    } else if (result < 0) {
      ert_log_error("ert_comm_protocol_transmit_stream_write failed with result: %d", result);
      goto error_close_file;
    }

    offset += read_bytes;
  } while (*running && read_result > 0);

  close(fd);

  uint32_t buffer_bytes_written = 0;
  if (data_length > 0 && data != NULL) {
    result = ert_comm_protocol_write_buffer(comm_protocol, stream, data_length, data, &buffer_bytes_written);
    if (result < 0) {
      goto error_close_stream;
    }
  }

  ert_log_debug("Closing stream at %d/%d of file '%s' ...", offset, filesize, filename);

  retry_count = 0;

  retry_close:
  result = ert_comm_protocol_transmit_stream_close(comm_protocol, stream, false);
  if (result == -EAGAIN && retry_count < TRANSMIT_RETRY_COUNT) {
    retry_count++;
    ert_log_info("Retrying close: %d of %d", retry_count, TRANSMIT_RETRY_COUNT);
    usleep(TRANSMIT_RETRY_DELAY_MILLIS * 1000);
    goto retry_close;
  } else if (result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result: %d", result);
    goto error_close_stream;
  }

  ert_log_info("Transmitted %d/%d bytes of file '%s' and %d/%d bytes of buffer successfully",
      offset, filesize, filename, buffer_bytes_written, data_length);

  return 0;

  error_close_file:
  close(fd);

  error_close_stream:
  close_result = ert_comm_protocol_transmit_stream_close(comm_protocol, stream, true);
  if (close_result < 0) {
    ert_log_error("ert_comm_protocol_transmit_stream_close failed with result: %d", close_result);
    return close_result;
  }

  return result;
}

int ert_comm_protocol_transmit_file(ert_comm_protocol *comm_protocol, uint8_t port,
    bool enable_acks, const char *filename, volatile bool *running)
{
  return ert_comm_protocol_transmit_file_and_buffer(comm_protocol, port, enable_acks, filename, 0, NULL, running);
}

int ert_comm_protocol_receive_buffer(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream,
    uint32_t buffer_size, uint8_t *buffer, uint32_t *bytes_received, volatile bool *running)
{
  ert_comm_protocol_stream_info stream_info;

  uint32_t total_bytes_read = 0;
  uint32_t bytes_read = 0;
  int result;

  ert_comm_protocol_stream_get_info(stream, &stream_info);

  ert_log_info("Starting to read data in stream ID %d port %d ...",
      stream_info.stream_id, stream_info.port);

  do {
    ert_log_debug("Reading data in stream ID %d port %d ...", stream_info.stream_id, stream_info.port);
    result = ert_comm_protocol_receive_stream_read(comm_protocol, stream, STREAM_READ_TIMEOUT_MILLIS,
        buffer_size - total_bytes_read, buffer + total_bytes_read, &bytes_read);
    if (result == -ETIMEDOUT) {
      ert_log_debug("Read timed out, retrying read for stream ID %d port %d ...", stream_info.stream_id, stream_info.port);
      continue;
    } else if (result < 0) {
      ert_log_error("ert_comm_protocol_receive_stream_read failed with result: %d", result);
      break;
    }

    total_bytes_read += bytes_read;
    ert_log_debug("Bytes received in packet for stream ID %d port %d: %d",
        stream_info.stream_id, stream_info.port, bytes_read);
  } while (*running && bytes_read > 0);

  if (result < 0) {
    ert_comm_protocol_receive_stream_close(comm_protocol, stream);
    return result;
  }

  result = ert_comm_protocol_receive_stream_close(comm_protocol, stream);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_receive_stream_close failed with result: %d", result);
    return result;
  }

  ert_log_info("Total bytes of data received for stream ID %d port %d: %d",
      stream_info.stream_id, stream_info.port, total_bytes_read);

  *bytes_received = total_bytes_read;

  return 0;
}

int ert_comm_protocol_receive_file(ert_comm_protocol *comm_protocol, ert_comm_protocol_stream *stream, const char *filename,
    bool delete_empty, volatile bool *running, uint32_t *bytes_received_rcv)
{
  int result;
  ert_comm_protocol_stream_info stream_info;

  uint32_t buffer_size = STREAM_FILE_DATA_BUFFER_LENGTH;
  uint8_t data[buffer_size];

  int fd = open(filename, O_WRONLY | O_CREAT);
  if (fd < 0) {
    ert_log_error("Error opening file '%s' for writing: %s", filename, strerror(errno));
    return -EIO;
  }

  uint32_t total_bytes_read = 0;
  uint32_t bytes_read = 0;

  ert_comm_protocol_stream_get_info(stream, &stream_info);

  ert_log_info("Starting to read data in stream ID %d port %d to file %s ...",
      stream_info.stream_id, stream_info.port, filename);

  do {
    ert_log_debug("Reading data in stream ID %d port %d to file %s ...",
        stream_info.stream_id, stream_info.port, filename);
    result = ert_comm_protocol_receive_stream_read(comm_protocol, stream, STREAM_READ_TIMEOUT_MILLIS,
        buffer_size, data, &bytes_read);
    if (result == -ETIMEDOUT) {
      ert_log_debug("Read timed out, retrying read ...");
      continue;
    } else if (result < 0) {
      ert_log_error("ert_comm_protocol_receive_stream_read failed with result: %d", result);
      break;
    }

    total_bytes_read += bytes_read;

    ert_log_debug("Bytes received in packet: %d in stream ID %d port %d to file %s",
        bytes_read, stream_info.stream_id, stream_info.port, filename);

    if (bytes_read > 0) {
      ssize_t write_result = write(fd, data, bytes_read);
      if (write_result < 0) {
        ert_log_error("Error writing file '%s': %s", filename, strerror(errno));
        result = -EIO;
        break;
      }
    }
  } while (*running && bytes_read > 0);

  close(fd);

  ert_log_info("Total bytes received: %d in stream ID %d port %d to file %s", total_bytes_read,
      stream_info.stream_id, stream_info.port, filename);

  if (delete_empty && (total_bytes_read == 0)) {
    ert_log_info("Deleting empty file: %s", filename);
    remove(filename);
  }

  if (result < 0) {
    ert_comm_protocol_receive_stream_close(comm_protocol, stream);
    return result;
  }

  result = ert_comm_protocol_receive_stream_close(comm_protocol, stream);
  if (result < 0) {
    ert_log_error("ert_comm_protocol_receive_stream_close failed with result: %d", result);
    return result;
  }

  *bytes_received_rcv = total_bytes_read;

  return 0;
}
