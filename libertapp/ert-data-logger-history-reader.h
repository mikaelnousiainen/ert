/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DATA_LOGGER_HISTORY_READER_H
#define __ERT_DATA_LOGGER_HISTORY_READER_H

#include "ert-common.h"

typedef struct _ert_data_logger_history_reader {
  FILE *fs;
  off_t file_size;

  uint32_t entries_read;
  uint32_t total_entry_count;

  uint32_t buffer_length;
  uint8_t *buffer;
} ert_data_logger_history_reader;

int ert_data_logger_file_history_list(uint8_t *path, uint8_t *extension, size_t count, uint32_t *buffer_length_rcv, uint8_t **buffer_rcv);

int ert_data_logger_history_reader_create_from_filename_template(char *path, char *filename_template,
    uint32_t buffer_length, uint32_t entry_count, ert_data_logger_history_reader **reader_rcv);
int ert_data_logger_history_reader_create(FILE *fs, uint32_t buffer_length, uint32_t entry_count,
    ert_data_logger_history_reader **reader_rcv);
int ert_data_logger_history_reader_destroy(ert_data_logger_history_reader *reader);
int ert_data_logger_history_reader_callback(void *callback_context, uint32_t *length, uint8_t **data);
int ert_data_logger_history_reader_callback_finished(void *callback_context);


#endif
