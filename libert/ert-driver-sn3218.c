/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// SN3218 LED driver code based on Pimoroni driver: https://github.com/pimoroni/sn3218/

#include <math.h>
#include <errno.h>
#include <string.h>

#include "ert-driver-sn3218.h"
#include "ert-log.h"

#define SN3218_I2C_ADDRESS 0x54
#define SN3218_COMMAND_ENABLE_OUTPUT 0x00
#define SN3218_COMMAND_SET_PWM_VALUES 0x01
#define SN3218_COMMAND_ENABLE_LEDS 0x13
#define SN3218_COMMAND_UPDATE 0x16
#define SN3218_COMMAND_RESET 0x17

int sn3218_set_enabled(ert_driver_sn3218 *driver, bool enabled)
{
  return hal_i2c_write_byte_command(driver->i2c_device, SN3218_COMMAND_ENABLE_OUTPUT, enabled ? 0x01 : 0x00);
}

int sn3218_reset(ert_driver_sn3218 *driver)
{
  return hal_i2c_write_byte_command(driver->i2c_device, SN3218_COMMAND_RESET, 0xFF);
}

int sn3218_update(ert_driver_sn3218 *driver)
{
  return hal_i2c_write_byte_command(driver->i2c_device, SN3218_COMMAND_UPDATE, 0xFF);
}

int sn3218_enable_leds(ert_driver_sn3218 *driver, uint32_t enable_mask)
{
  uint8_t values[3] = {
      (uint8_t) (enable_mask & 0x3F),
      (uint8_t) ((enable_mask >> 6) & 0x3F),
      (uint8_t) ((enable_mask >> 12) & 0X3F),
  };

  int result = hal_i2c_write_i2c_block(driver->i2c_device, SN3218_COMMAND_ENABLE_LEDS, 3, values);
  if (result < 0) {
    return result;
  }

  return sn3218_update(driver);
}

int sn3218_set_led(ert_driver_sn3218 *driver, uint8_t index, uint8_t value)
{
  if (index >= SN3218_LED_COUNT) {
    return -EINVAL;
  }

  uint8_t gamma_value = driver->gamma_table[index][value];

  int result = hal_i2c_write_i2c_block(driver->i2c_device, SN3218_COMMAND_SET_PWM_VALUES + index, 1, &gamma_value);
  if (result < 0) {
    return result;
  }

  return sn3218_update(driver);
}

int sn3218_set_leds(ert_driver_sn3218 *driver, uint8_t length, uint8_t *values)
{
  if (length > SN3218_LED_COUNT) {
    return -EINVAL;
  }

  uint8_t gamma_values[SN3218_LED_COUNT];

  for (uint8_t i = 0; i < length; i++) {
    gamma_values[i] = driver->gamma_table[i][values[i]];
  }

  int result = hal_i2c_write_i2c_block(driver->i2c_device, SN3218_COMMAND_SET_PWM_VALUES, length, gamma_values);
  if (result < 0) {
    return result;
  }

  return sn3218_update(driver);
}

int sn3218_reset_gamma_table(ert_driver_sn3218 *driver)
{
  for (uint16_t v = 0; v < 256; v++) {
    float value = powf(255.0f, ((float) v - 1.0f) / 255.0f);

    for (uint16_t i = 0; i < SN3218_LED_COUNT; i++) {
      driver->gamma_table[i][v] = (uint8_t) value;
    }
  }

  return 0;
}

inline uint8_t sn3218_get_gamma_table_value(ert_driver_sn3218 *driver, uint8_t led_index, uint8_t value_index)
{
  if (led_index >= SN3218_LED_COUNT) {
    return 0;
  }

  return driver->gamma_table[led_index][value_index];
}

inline int sn3218_set_gamma_table_value(ert_driver_sn3218 *driver, uint8_t led_index, uint8_t value_index, uint8_t value)
{
  if (led_index >= SN3218_LED_COUNT) {
    return -EINVAL;
  }

  driver->gamma_table[led_index][value_index] = value;

  return 0;
}

int sn3218_open(int bus, ert_driver_sn3218 **driver_rcv)
{
  int result;
  ert_driver_sn3218 *driver;

  driver = calloc(1, sizeof(ert_driver_sn3218));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for SN3218 driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  result = hal_i2c_open(bus, SN3218_I2C_ADDRESS, &driver->i2c_device);
  if (result != 0) {
    free(driver);
    ert_log_error("Error opening I2C device");
    return result;
  }

  sn3218_reset_gamma_table(driver);

  *driver_rcv = driver;

  return 0;
}

int sn3218_close(ert_driver_sn3218 *driver)
{
  int result;

  result = hal_i2c_close(driver->i2c_device);

  free(driver);

  return result;
}
