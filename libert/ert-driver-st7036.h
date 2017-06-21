/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_ST7036_H
#define __ERT_DRIVER_ST7036_H

#include "ert-common.h"
#include "ert-hal-spi.h"

#define ERT_DRIVER_ST7036_CONTRAST_DEFAULT 40
#define ERT_DRIVER_ST7036_FOLLOWER_CIRCUIT_AMPLIFIED_RATIO_DEFAULT 3

#define ERT_DRIVER_ST7036_BIAS_1_4 14
#define ERT_DRIVER_ST7036_BIAS_1_5 15

typedef struct _ert_driver_st7036_static_config {
  uint16_t spi_bus_index;
  uint16_t spi_device_index;
  uint32_t spi_clock_speed;

  uint8_t pin_register_select;
  uint8_t pin_reset;

  uint8_t columns;
  uint8_t rows;

  uint8_t bias;
  bool power_boost;
  bool follower_circuit;
  uint8_t follower_circuit_amplified_ratio;
  uint8_t initial_contrast;
} ert_driver_st7036_static_config;

typedef struct _ert_driver_st7036 {
  hal_spi_device *spi_device;

  bool enabled;
  bool cursor_enabled;
  bool cursor_blink;
  bool double_height;

  bool shift_cursor_right;
  bool shift_display;

  ert_driver_st7036_static_config static_config;
} ert_driver_st7036;

int st7036_reset(ert_driver_st7036 *driver);
int st7036_clear(ert_driver_st7036 *driver);
int st7036_home(ert_driver_st7036 *driver);
int st7036_set_contrast(ert_driver_st7036 *driver, uint8_t contrast);

int st7036_set_display_enabled(ert_driver_st7036 *driver, bool enabled);
int st7036_set_cursor_enabled(ert_driver_st7036 *driver, bool cursor_enabled);
int st7036_set_cursor_blink(ert_driver_st7036 *driver, bool cursor_blink);

int st7036_set_cursor_position(ert_driver_st7036 *driver, uint8_t column, uint8_t row);
int st7036_shift_cursor_left(ert_driver_st7036 *driver);
int st7036_shift_cursor_right(ert_driver_st7036 *driver);
int st7036_shift_screen_left(ert_driver_st7036 *driver);
int st7036_shift_screen_right(ert_driver_st7036 *driver);

int st7036_write(ert_driver_st7036 *driver, char *text);

int st7036_open(ert_driver_st7036_static_config *static_config, ert_driver_st7036 **driver_rcv);
int st7036_close(ert_driver_st7036 *driver);

#endif
