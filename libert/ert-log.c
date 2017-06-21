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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <zlog.h>

#include "ert-log.h"

struct _ert_log_logger {
  zlog_category_t *category;
};

int ert_log_init(const char *log_config_file)
{
  return dzlog_init(log_config_file, "ert");
}

int ert_log_uninit()
{
  zlog_fini();
  return 0;
}

static inline int ert_log_convert_to_zlog_level(ert_log_level level) {
  switch (level) {
    case ERT_LOG_LEVEL_DEBUG:
      return ZLOG_LEVEL_DEBUG;
    case ERT_LOG_LEVEL_INFO:
      return ZLOG_LEVEL_INFO;
    case ERT_LOG_LEVEL_NOTICE:
      return ZLOG_LEVEL_NOTICE;
    case ERT_LOG_LEVEL_WARN:
      return ZLOG_LEVEL_WARN;
    case ERT_LOG_LEVEL_ERROR:
      return ZLOG_LEVEL_ERROR;
    case ERT_LOG_LEVEL_FATAL:
      return ZLOG_LEVEL_FATAL;
    default:
      return ZLOG_LEVEL_ERROR;
  }
}

void ert_log(const char *file, size_t filelen, const char *func, size_t funclen, long line, ert_log_level level,
    const char *format, ...)
{
  va_list argp;

  int zlog_level = ert_log_convert_to_zlog_level(level);
  va_start(argp, format);
  vdzlog(file, filelen, func, funclen, line, zlog_level, format, argp);
  va_end(argp);

  if (level == ERT_LOG_LEVEL_FATAL) {
    ert_log_uninit();
    exit(EXIT_FAILURE);
  }
}

int ert_log_logger_create(const char *category_name, ert_log_logger **logger_rcv)
{
  ert_log_logger *logger = calloc(1, sizeof(ert_log_logger));
  if (logger == NULL) {
    ert_log_fatal("Error allocating memory for logger struct: %s", strerror(errno));
    return -ENOMEM;
  }

  logger->category = zlog_get_category(category_name);

  *logger_rcv = logger;

  return 0;
}

int ert_log_logger_destroy(ert_log_logger *logger)
{
  free(logger);

  return 0;
}

void ert_log_logger_log(ert_log_logger *logger, const char *file, size_t filelen,
    const char *func, size_t funclen, long line, ert_log_level level, const char *format, ...)
{
  va_list argp;

  int zlog_level = ert_log_convert_to_zlog_level(level);
  va_start(argp, format);
  vzlog(logger->category, file, filelen, func, funclen, line, zlog_level, format, argp);
  va_end(argp);

  if (level == ERT_LOG_LEVEL_FATAL) {
    ert_log_uninit();
    exit(EXIT_FAILURE);
  }
}
