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

#include "ert-driver-dothat-led.h"
#include "ert-log.h"

uint8_t dothat_led_get_led_count(ert_driver_dothat_led *driver)
{
  return cap1xxx_get_led_count(driver->cap1xxx);
}

int dothat_led_set_brightness(ert_driver_dothat_led *driver, uint8_t value)
{
  return cap1xxx_set_led_direct_duty(driver->cap1xxx, 0, value);
}

int dothat_led_set_ramp_rate(ert_driver_dothat_led *driver, uint32_t millis)
{
  return cap1xxx_set_led_direct_ramp_rate(driver->cap1xxx, millis, millis);
}

int dothat_led_off(ert_driver_dothat_led *driver)
{
  return cap1xxx_set_led_state_all(driver->cap1xxx, 0x00);
}

int dothat_led_set_led_state(ert_driver_dothat_led *driver, uint8_t led_index, bool on)
{
  return cap1xxx_set_led_state(driver->cap1xxx, led_index, on);
}

int dothat_led_invert_graph(ert_driver_dothat_led *driver, bool invert)
{
  driver->invert_graph = invert;
  return 0;
}

int dothat_led_set_led_graph(ert_driver_dothat_led *driver, float percentage)
{
  uint8_t led_count = dothat_led_get_led_count(driver);

  float step = 100.0f / (float) led_count;

  uint8_t led_state_mask = 0x00;
  float value = 0.0f;
  for (uint8_t led_index = 0; led_index < led_count; led_index++, value += step) {
    if (value < percentage) {
      if (driver->invert_graph) {
        led_state_mask |= (1 << (led_count - led_index - 1));
      } else {
        led_state_mask |= (1 << led_index);
      }
    }
  }

  return cap1xxx_set_led_state_all(driver->cap1xxx, led_state_mask);
}

int dothat_led_open(ert_driver_cap1xxx *cap1xxx, ert_driver_dothat_led **driver_rcv)
{
  ert_driver_dothat_led *driver;

  driver = calloc(1, sizeof(ert_driver_dothat_led));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for DOT HAT LED driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  driver->cap1xxx = cap1xxx;
  driver->invert_graph = true;

  cap1xxx_set_led_direct_ramp_rate(driver->cap1xxx, 0, 0);
  uint8_t led_count = cap1xxx_get_led_count(driver->cap1xxx);
  for (uint8_t i = 0; i < led_count; i++) {
    cap1xxx_set_led_behavior(driver->cap1xxx, i, CAP1XXX_REGISTER_LED_BEHAVIOUR_DIRECT);
    cap1xxx_set_led_polarity(driver->cap1xxx, i, false);
  }

  *driver_rcv = driver;

  return 0;
}

int dothat_led_close(ert_driver_dothat_led *driver)
{
  free(driver);

  return 0;
}
