/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_DOTHAT_BACKLIGHT_H
#define __ERT_DRIVER_DOTHAT_BACKLIGHT_H

#include "ert-driver-sn3218.h"

#define DOTHAT_BACKLIGHT_RGB_LED_COUNT (SN3218_LED_COUNT / 3)

typedef struct _ert_driver_dothat_backlight {
  ert_driver_sn3218 *sn3218;
  uint8_t led_values[SN3218_LED_COUNT]; // Led order: 6 x B G R
} ert_driver_dothat_backlight;

int dothat_backlight_update(ert_driver_dothat_backlight *driver);
int dothat_backlight_set_rgb_state(ert_driver_dothat_backlight *driver, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_set_rgb(ert_driver_dothat_backlight *driver, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_set_rgb_all(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_set_rgb_left(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_set_rgb_mid(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_set_rgb_right(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b);
int dothat_backlight_off(ert_driver_dothat_backlight *driver);
int dothat_backlight_set_enabled(ert_driver_dothat_backlight *driver, bool enabled);
int dothat_backlight_open(int bus, ert_driver_dothat_backlight **driver_rcv);
int dothat_backlight_close(ert_driver_dothat_backlight *driver);

#endif
