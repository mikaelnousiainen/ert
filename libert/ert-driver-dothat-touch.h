/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_DOTHAT_TOUCH_H
#define __ERT_DRIVER_DOTHAT_TOUCH_H

#include "ert-driver-cap1xxx.h"

#define DOTHAT_TOUCH_I2C_ADDRESS 0x2C

typedef enum _dothat_touch_button {
  DOTHAT_TOUCH_BUTTON_CANCEL = 0,
  DOTHAT_TOUCH_BUTTON_UP = 1,
  DOTHAT_TOUCH_BUTTON_DOWN = 2,
  DOTHAT_TOUCH_BUTTON_LEFT = 3,
  DOTHAT_TOUCH_BUTTON_SELECT = 4,
  DOTHAT_TOUCH_BUTTON_RIGHT = 5,
} dothat_touch_button;

typedef struct _ert_driver_dothat_touch {
  ert_driver_cap1xxx *cap1xxx;
  ert_driver_cap1xxx_input_state input_state[CAP1XXX_INPUT_COUNT_MAX];
} ert_driver_dothat_touch;

uint8_t dothat_touch_get_input_count(ert_driver_dothat_touch *driver);
int dothat_touch_set_repeat_state(ert_driver_dothat_touch *driver, bool enable);
int dothat_touch_set_repeat_rate(ert_driver_dothat_touch *driver, uint16_t milliseconds);
int dothat_touch_read(ert_driver_dothat_touch *driver);
int dothat_touch_wait(ert_driver_dothat_touch *driver, uint32_t timeout_millis);
ert_driver_cap1xxx_input_state dothat_touch_get_button_state(ert_driver_dothat_touch *driver, dothat_touch_button button);
int dothat_touch_is_pressed(ert_driver_dothat_touch *driver, dothat_touch_button button);
int dothat_touch_is_held(ert_driver_dothat_touch *driver, dothat_touch_button button);
int dothat_touch_is_released(ert_driver_dothat_touch *driver, dothat_touch_button button);
int dothat_touch_open(ert_driver_cap1xxx *cap1xxx, ert_driver_dothat_touch **driver_rcv);
int dothat_touch_close(ert_driver_dothat_touch *driver);

#endif