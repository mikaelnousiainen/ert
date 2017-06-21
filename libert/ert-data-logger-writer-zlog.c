/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <zlog.h>
#include <string.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-data-logger-writer-zlog.h"

typedef struct _ert_data_logger_writer_zlog {
  zlog_category_t *category;
} ert_data_logger_writer_zlog;

int ert_data_logger_writer_zlog_write(ert_data_logger_writer *writer, uint32_t length, uint8_t *data)
{
  ert_data_logger_writer_zlog *writer_zlog = (ert_data_logger_writer_zlog *) writer->priv;

  zlog_info(writer_zlog->category, "%s", data);

  return 0;
}

int ert_data_logger_writer_zlog_create(const char *category_name, ert_data_logger_writer **writer_rcv)
{
  ert_data_logger_writer *writer = calloc(1, sizeof(ert_data_logger_writer));
  if (writer == NULL) {
    ert_log_fatal("Error allocating memory for writer struct: %s", strerror(errno));
    return -ENOMEM;
  }

  ert_data_logger_writer_zlog *writer_zlog = calloc(1, sizeof(ert_data_logger_writer_zlog));
  if (writer_zlog == NULL) {
    free(writer);
    ert_log_fatal("Error allocating memory for zlog writer struct: %s", strerror(errno));
    return -ENOMEM;
  }

  writer_zlog->category = zlog_get_category(category_name);

  writer->write = ert_data_logger_writer_zlog_write;
  writer->priv = writer_zlog;

  *writer_rcv = writer;

  return 0;
}

int ert_data_logger_writer_zlog_destroy(ert_data_logger_writer *writer)
{
  free(writer->priv);
  free(writer);

  return 0;
}
