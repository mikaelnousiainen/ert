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

#include "ert-driver-dothat-touch.h"
#include "ert-log.h"

uint8_t dothat_touch_get_input_count(ert_driver_dothat_touch *driver)
{
  return cap1xxx_get_input_count(driver->cap1xxx);
}

int dothat_touch_set_repeat_state(ert_driver_dothat_touch *driver, bool enable)
{
  return cap1xxx_enable_repeat(driver->cap1xxx, (uint8_t) (enable ? 0xFF : 0x00));
}

int dothat_touch_set_repeat_rate(ert_driver_dothat_touch *driver, uint16_t milliseconds)
{
  return cap1xxx_set_repeat_rate(driver->cap1xxx, milliseconds);
}

int dothat_touch_read(ert_driver_dothat_touch *driver)
{
  return cap1xxx_get_input_state(driver->cap1xxx, driver->input_state);
}

int dothat_touch_wait(ert_driver_dothat_touch *driver, uint32_t timeout_millis)
{
  return cap1xxx_wait_for_input(driver->cap1xxx, timeout_millis, driver->input_state);
}

inline ert_driver_cap1xxx_input_state dothat_touch_get_button_state(ert_driver_dothat_touch *driver, dothat_touch_button button)
{
  return driver->input_state[button];
}

inline int dothat_touch_is_pressed(ert_driver_dothat_touch *driver, dothat_touch_button button)
{
  return driver->input_state[button] == CAP1XXX_INPUT_STATE_PRESSED;
}

inline int dothat_touch_is_held(ert_driver_dothat_touch *driver, dothat_touch_button button)
{
  return driver->input_state[button] == CAP1XXX_INPUT_STATE_HELD;
}

inline int dothat_touch_is_released(ert_driver_dothat_touch *driver, dothat_touch_button button)
{
  return driver->input_state[button] == CAP1XXX_INPUT_STATE_RELEASED;
}

int dothat_touch_open(ert_driver_cap1xxx *cap1xxx, ert_driver_dothat_touch **driver_rcv)
{
  ert_driver_dothat_touch *driver;

  driver = calloc(1, sizeof(ert_driver_dothat_touch));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for DOT HAT touch driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  driver->cap1xxx = cap1xxx;

  *driver_rcv = driver;

  return 0;
}

int dothat_touch_close(ert_driver_dothat_touch *driver)
{
  free(driver);

  return 0;
}
