/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_LOG_H
#define __ERT_LOG_H

#include "ert-common.h"

typedef enum _ert_log_level {
  ERT_LOG_LEVEL_DEBUG = 10,
  ERT_LOG_LEVEL_INFO = 20,
  ERT_LOG_LEVEL_NOTICE = 30,
  ERT_LOG_LEVEL_WARN = 40,
  ERT_LOG_LEVEL_ERROR = 100,
  ERT_LOG_LEVEL_FATAL = 1000
} ert_log_level;

typedef struct _ert_log_logger ert_log_logger;

#ifdef __cplusplus
extern "C" {
#endif

int ert_log_init(const char *log_config_file);
int ert_log_uninit();

void ert_log(const char *file, size_t filelen, const char *func, size_t funclen, long line, ert_log_level level,
    const char *format, ...);

int ert_log_logger_create(const char *category_name, ert_log_logger **logger_rcv);
int ert_log_logger_destroy(ert_log_logger *logger);
void ert_log_logger_log(ert_log_logger *logger, const char *file, size_t filelen,
		const char *func, size_t funclen, long line, ert_log_level level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#define ert_log_with_level(level, ...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	level, __VA_ARGS__)

#define ert_log_fatal(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_FATAL, __VA_ARGS__)
#define ert_log_error(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_ERROR, __VA_ARGS__)
#define ert_log_warn(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_WARN, __VA_ARGS__)
#define ert_log_notice(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_NOTICE, __VA_ARGS__)
#define ert_log_info(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_INFO, __VA_ARGS__)

#define ert_logl_with_level(logger, level, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	level, __VA_ARGS__)

#define ert_logl_fatal(logger, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_FATAL, __VA_ARGS__)
#define ert_logl_error(logger, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_ERROR, __VA_ARGS__)
#define ert_logl_warn(logger, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_WARN, __VA_ARGS__)
#define ert_logl_notice(logger, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_NOTICE, __VA_ARGS__)
#define ert_logl_info(logger, ...) \
	ert_log_logger_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_INFO, __VA_ARGS__)

#ifdef ERTLIB_ENABLE_LOG_DEBUG
#define ert_log_debug(...) \
	ert_log(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_DEBUG, __VA_ARGS__)
#define ert_logl_debug(logger, ...) \
	ert_log(logger, __FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
	ERT_LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define ert_log_debug(...)
#define ert_logl_debug(...)
#endif

#endif
