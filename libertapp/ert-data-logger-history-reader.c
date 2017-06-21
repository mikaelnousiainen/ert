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
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "ert-data-logger-history-reader.h"
#include "ert-log.h"
#include "ert-fileutil.h"

int ert_data_logger_file_history_list(uint8_t *path, uint8_t *extension, size_t count, uint32_t *buffer_length_rcv, uint8_t **buffer_rcv)
{
  char path_realpath[PATH_MAX];
  if (realpath(path, path_realpath) == NULL) {
    ert_log_error("Invalid path for listing file history: %s (%s)", path, strerror(errno));
    return -EINVAL;
  }

  struct dirent **namelist;
  int result = scandir((char *) path_realpath, &namelist, NULL, alphasort);
  if (result < 0) {
    ert_log_error("Error listing files in path: %s (%s)", path_realpath, strerror(errno));
    return -EIO;
  }

  uint32_t buffer_length = 0;
  for (int i = 0; i < result; i++) {
    int index = result - i - 1; // Reverse order
    char *name = namelist[index]->d_name;

    if (i < count && namelist[index]->d_type == DT_REG) {
      buffer_length += strlen(name) + 8;
    }
  }

  uint8_t *buffer = malloc(buffer_length);
  if (buffer == NULL) {
    for (int i = 0; i < result; i++) {
      free(namelist[i]);
    }
    free(namelist);
    ert_log_fatal("Error allocating memory for data logger file history list buffer: %s", strerror(errno));
    return -ENOMEM;
  }

  size_t buffer_offset = 0;
  for (int i = 0; i < result; i++) {
    int index = result - i - 1; // Reverse order
    char *name = namelist[index]->d_name;

    if (i < count && namelist[index]->d_type == DT_REG) {
      uint8_t sep = (uint8_t) ((i == 0) ? '[' : ',');

      buffer[buffer_offset] = sep;
      buffer_offset++;

      buffer_offset += snprintf((char *) buffer + buffer_offset, buffer_length - buffer_offset,
          "\"%s\"", name);
    }

    free(namelist[index]);
  }
  free(namelist);

  buffer[buffer_offset] = ']';
  buffer_offset++;

  *buffer_rcv = buffer;
  *buffer_length_rcv = (uint32_t) buffer_offset;

  return 0;
}

static int format_filename_template(char *format, size_t length, char *filename, int32_t offset_days)
{
  struct timespec ts;

  int result = clock_gettime(CLOCK_REALTIME, &ts);
  if (result < 0) {
    ert_log_error("Error getting current time: %s", strerror(errno));
    return -EIO;
  }

  ts.tv_sec += offset_days * (24 * 60 * 60);

  struct tm timestamp_tm;

  gmtime_r(&ts.tv_sec, &timestamp_tm);

  strftime(filename, length, format, &timestamp_tm);

  return 0;
}

int ert_data_logger_history_reader_create_from_filename_template(char *path, char *filename_template,
    uint32_t buffer_length, uint32_t entry_count, ert_data_logger_history_reader **reader_rcv)
{
  int result;

  char path_realpath[PATH_MAX];
  if (realpath(path, path_realpath) == NULL) {
    ert_log_error("Error resolving path: %s", path);
    return -EIO;
  }


  char full_filename[PATH_MAX];
  bool found = false;

  for (uint32_t offset_days = 0; offset_days < 365; offset_days++) {
    char filename[PATH_MAX];

    result = format_filename_template(filename_template, PATH_MAX, filename, -offset_days);
    if (result < 0) {
      return -EIO;
    }

    snprintf(full_filename, PATH_MAX, "%s/%s", path_realpath, filename);

    if (access(full_filename, F_OK | R_OK) == 0) {
      found = true;
      break;
    }
  }

  if (!found) {
    ert_log_info("No files found in data logger history, last one checked: %s", full_filename);
    return -ENOENT;
  }

  FILE *fs = fopen(full_filename, "rb");
  if (fs == NULL) {
    ert_log_warn("Error opening data logger history file: %s", full_filename);
    return -ENOENT;
  }

  return ert_data_logger_history_reader_create(fs, buffer_length, entry_count, reader_rcv);
}

int ert_data_logger_history_reader_create(FILE *fs, uint32_t buffer_length, uint32_t entry_count,
    ert_data_logger_history_reader **reader_rcv)
{
  off_t file_size = fsize(fs);
  if (file_size < 0) {
    ert_log_error("Data logger file too long");
    return -EIO;
  }

  ert_data_logger_history_reader *reader = calloc(1, sizeof(ert_data_logger_history_reader));
  if (reader == NULL) {
    fclose(fs);
    ert_log_fatal("Error allocating memory for data logger history reader struct: %s", strerror(errno));
    return -ENOMEM;
  }

  reader->buffer = calloc(1, buffer_length);
  if (reader->buffer == NULL) {
    free(reader);
    fclose(fs);
    ert_log_fatal("Error allocating memory for data logger history reader buffer: %s", strerror(errno));
    return -ENOMEM;
  }


  reader->fs = fs;
  reader->file_size = file_size;
  reader->entries_read = 0;
  reader->total_entry_count = entry_count;
  reader->buffer_length = buffer_length;

  *reader_rcv = reader;

  return 0;
}

int ert_data_logger_history_reader_destroy(ert_data_logger_history_reader *reader)
{
  fclose(reader->fs);
  free(reader->buffer);
  free(reader);

  return 0;
}

int ert_data_logger_history_reader_callback(void *callback_context, uint32_t *length, uint8_t **data)
{
  ert_data_logger_history_reader *reader = (ert_data_logger_history_reader *) callback_context;
  uint32_t bytes_to_read = (*length > reader->buffer_length) ? reader->buffer_length : *length;

  if (reader->entries_read >= reader->total_entry_count) {
    *length = 0;
    return 0;
  }

  off_t file_offset = ftello(reader->fs);
  if (file_offset == 0) {
    *length = 0;
    return 0;
  }

  uint32_t data_offset = 0;
  bool first = reader->entries_read == 0;

  if (first) {
    // First
    reader->buffer[0] = '[';
    data_offset = 1;
  } else {
    reader->buffer[0] = ',';
    data_offset = 1;
  }

  size_t bytes_read = fgetsr(reader->fs, bytes_to_read - data_offset - 1, (char *) reader->buffer + data_offset);

  reader->entries_read++;

  file_offset = ftello(reader->fs);
  bool last = (reader->entries_read >= reader->total_entry_count) || (file_offset == 0);

  uint32_t data_trailer = 0;
  if (last) {
    reader->buffer[bytes_read + data_offset] = ']';
    data_trailer = 1;
  }

  *data = reader->buffer;
  *length = (uint32_t) bytes_read + data_offset + data_trailer;

  return 0;
}

int ert_data_logger_history_reader_callback_finished(void *callback_context)
{
  ert_data_logger_history_reader *reader = (ert_data_logger_history_reader *) callback_context;
  ert_data_logger_history_reader_destroy(reader);
  return 0;
}
