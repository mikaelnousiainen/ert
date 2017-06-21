/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <errno.h>
#include <string.h>

#include "ert-driver-dothat-backlight.h"
#include "ert-log.h"

static void dothat_backlight_set_default_gamma_table(ert_driver_dothat_backlight *driver)
{
  for (uint8_t i = 0; i < SN3218_LED_COUNT; i += 3) {
    for (uint16_t v = 0; v < 256; v++) {
      uint8_t current_b = sn3218_get_gamma_table_value(driver->sn3218, i, (uint8_t) v);
      sn3218_set_gamma_table_value(driver->sn3218, i, (uint8_t) v, (uint8_t) ((float) current_b / 1.6f));

      uint8_t current_g = sn3218_get_gamma_table_value(driver->sn3218, i + 1, (uint8_t) v);
      sn3218_set_gamma_table_value(driver->sn3218, i + 1, (uint8_t) v, (uint8_t) ((float) current_g / 1.4f));
    }
  }
}

inline int dothat_backlight_update(ert_driver_dothat_backlight *driver)
{
  return sn3218_set_leds(driver->sn3218, SN3218_LED_COUNT, driver->led_values);
}

inline int dothat_backlight_set_rgb_state(ert_driver_dothat_backlight *driver, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
  if (led_index >= DOTHAT_BACKLIGHT_RGB_LED_COUNT) {
    return -EINVAL;
  }

  driver->led_values[led_index * 3] = b;
  driver->led_values[led_index * 3 + 1] = g;
  driver->led_values[led_index * 3 + 2] = r;

  return 0;
}

int dothat_backlight_set_rgb(ert_driver_dothat_backlight *driver, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
  int result = dothat_backlight_set_rgb_state(driver, led_index, r, g, b);
  if (result < 0) {
    return result;
  }

  return dothat_backlight_update(driver);
}

int dothat_backlight_set_rgb_all(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b)
{
  for (uint8_t i = 0; i < DOTHAT_BACKLIGHT_RGB_LED_COUNT; i++) {
    dothat_backlight_set_rgb_state(driver, i, r, g, b);
  }

  return dothat_backlight_update(driver);
}

int dothat_backlight_set_rgb_left(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b)
{
  dothat_backlight_set_rgb_state(driver, 0, r, g, b);
  dothat_backlight_set_rgb_state(driver, 1, r, g, b);

  return dothat_backlight_update(driver);
}

int dothat_backlight_set_rgb_mid(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b)
{
  dothat_backlight_set_rgb_state(driver, 2, r, g, b);
  dothat_backlight_set_rgb_state(driver, 3, r, g, b);

  return dothat_backlight_update(driver);
}

int dothat_backlight_set_rgb_right(ert_driver_dothat_backlight *driver, uint8_t r, uint8_t g, uint8_t b)
{
  dothat_backlight_set_rgb_state(driver, 4, r, g, b);
  dothat_backlight_set_rgb_state(driver, 5, r, g, b);

  return dothat_backlight_update(driver);
}

int dothat_backlight_off(ert_driver_dothat_backlight *driver)
{
  for (uint8_t i = 0; i < SN3218_LED_COUNT; i++) {
    driver->led_values[i] = 0;
  }

  return dothat_backlight_update(driver);
}

int dothat_backlight_set_enabled(ert_driver_dothat_backlight *driver, bool enabled)
{
  int result = sn3218_set_enabled(driver->sn3218, enabled);
  if (result < 0) {
    return result;
  }

  if (enabled) {
    sn3218_enable_leds(driver->sn3218, 0xFFFFFFFF);
    return dothat_backlight_update(driver);
  }

  return 0;
}

int dothat_backlight_open(int bus, ert_driver_dothat_backlight **driver_rcv)
{
  ert_driver_dothat_backlight *driver;

  driver = calloc(1, sizeof(ert_driver_dothat_backlight));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for DOT HAT backlight driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  int result = sn3218_open(bus, &driver->sn3218);
  if (result != 0) {
    free(driver);
    ert_log_error("Error opening SN3218 driver for DOT HAT backlight: %s", strerror(errno));
    return result;
  }

  dothat_backlight_set_default_gamma_table(driver);
  dothat_backlight_set_enabled(driver, true);

  *driver_rcv = driver;

  return 0;
}

int dothat_backlight_close(ert_driver_dothat_backlight *driver)
{
  int result;

  result = sn3218_close(driver->sn3218);

  free(driver);

  return result;
}
