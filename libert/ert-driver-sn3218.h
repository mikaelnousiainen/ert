/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_SN3218_H
#define __ERT_DRIVER_SN3218_H

#include "ert-hal-i2c.h"

#define SN3218_LED_COUNT 18

typedef struct _ert_driver_sn3218 {
  hal_i2c_device *i2c_device;
  uint8_t gamma_table[SN3218_LED_COUNT][256];
} ert_driver_sn3218;

int sn3218_set_enabled(ert_driver_sn3218 *driver, bool enabled);
int sn3218_reset(ert_driver_sn3218 *driver);
int sn3218_update(ert_driver_sn3218 *driver);
int sn3218_enable_leds(ert_driver_sn3218 *driver, uint32_t enable_mask);
int sn3218_set_led(ert_driver_sn3218 *driver, uint8_t index, uint8_t value);
int sn3218_set_leds(ert_driver_sn3218 *driver, uint8_t length, uint8_t *values);
int sn3218_reset_gamma_table(ert_driver_sn3218 *driver);
uint8_t sn3218_get_gamma_table_value(ert_driver_sn3218 *driver, uint8_t led_index, uint8_t value_index);
int sn3218_set_gamma_table_value(ert_driver_sn3218 *driver, uint8_t led_index, uint8_t value_index, uint8_t value);
int sn3218_open(int bus, ert_driver_sn3218 **driver_rcv);
int sn3218_close(ert_driver_sn3218 *driver);

#endif
