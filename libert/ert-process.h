/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_PROCESS_H
#define __ERT_PROCESS_H

#include "ert-common.h"

int ert_process_run(const char *command, const char *args[], int *status, size_t output_buffer_length,
    char *output_buffer, size_t *bytes_read);
int ert_process_run_command(const char *command, const char *args[]);
void ert_process_sleep_with_interrupt(unsigned int seconds, volatile bool *running);
void ert_process_backtrace_handler(int sig);
void ert_process_register_backtrace_handler();

#endif
