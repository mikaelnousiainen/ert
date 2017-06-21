/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_DOTHAT_LED_H
#define __ERT_DRIVER_DOTHAT_LED_H

#include "ert-driver-cap1xxx.h"

#define DOTHAT_LED_I2C_ADDRESS 0x2C

#define DOTHAT_LED_BRIGHTNESS_MIN CAP1XXX_LED_DUTY_MIN
#define DOTHAT_LED_BRIGHTNESS_MAX CAP1XXX_LED_DUTY_MAX

typedef struct _ert_driver_dothat_led {
  ert_driver_cap1xxx *cap1xxx;
  bool invert_graph;
} ert_driver_dothat_led;

uint8_t dothat_led_get_led_count(ert_driver_dothat_led *driver);
int dothat_led_set_brightness(ert_driver_dothat_led *driver, uint8_t value);
int dothat_led_set_ramp_rate(ert_driver_dothat_led *driver, uint32_t millis);
int dothat_led_off(ert_driver_dothat_led *driver);
int dothat_led_set_led_state(ert_driver_dothat_led *driver, uint8_t led_index, bool on);
int dothat_led_set_led_graph(ert_driver_dothat_led *driver, float percentage);
int dothat_led_open(ert_driver_cap1xxx *cap1xxx, ert_driver_dothat_led **driver_rcv);
int dothat_led_close(ert_driver_dothat_led *driver);

#endif
