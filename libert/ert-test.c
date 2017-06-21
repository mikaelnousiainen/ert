/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-test.h"
#include "ert-process.h"
#include "ert-log.h"

int ert_test_init()
{
  ert_process_register_backtrace_handler();

  int result = ert_log_init("zlog.conf");

  return result;
}

int ert_test_uninit()
{
  ert_log_uninit();
}
