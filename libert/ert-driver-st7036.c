/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// ST7036 LCD driver code based on Pimoroni driver: https://github.com/pimoroni/st7036

#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <errno.h>

#include "ert-log.h"
#include "ert-hal-gpio.h"
#include "ert-driver-st7036.h"

#define MILLISECOND 1000
#define MICROSECOND 1

#define ST7036_COMMAND_CLEAR_DISPLAY     0x01
#define ST7036_COMMAND_RETURN_HOME       0x02
#define ST7036_COMMAND_SET_ENTRY_MODE    0x04
#define ST7036_COMMAND_SET_DISPLAY_MODE  0x08
#define ST7036_COMMAND_DISPLAY_SHIFT     0x10
#define ST7036_COMMAND_FUNCTION_SET      0x20
#define ST7036_COMMAND_SET_CGRAM         0x40
#define ST7036_COMMAND_SET_DDRAM_ADDR    0x80

#define ST7036_COMMAND_BIAS              0x14
#define ST7036_COMMAND_POWER_ICON_CONTRAST 0x50
#define ST7036_COMMAND_FOLLOWER          0x60
#define ST7036_COMMAND_CONTRAST_LSB      0x70

#define DISPLAY_MODE_ENABLED             0x04
#define DISPLAY_MODE_CURSOR_ENABLED      0x02
#define DISPLAY_MODE_CURSOR_BLINK        0x01

#define ENTRY_MODE_INC_DDRAM_ADDR        0x02
#define ENTRY_MODE_DEC_DDRAM_ADDR        0x00
#define ENTRY_MODE_SHIFT_DISPLAY         0x01

#define DISPLAY_SHIFT_SHIFT_SCREEN       0x08
#define DISPLAY_SHIFT_SHIFT_CURSOR       0x00
#define DISPLAY_SHIFT_RIGHT              0x04
#define DISPLAY_SHIFT_LEFT               0x00

#define FUNCTION_SET_INSTRUCTION_SET_NORMAL 0x00
#define FUNCTION_SET_INSTRUCTION_SET_EXT_1  0x01
#define FUNCTION_SET_INSTRUCTION_SET_EXT_2  0x02
#define FUNCTION_SET_DOUBLE_HEIGHT          0x04
#define FUNCTION_SET_LINE_COUNT_1           0x00
#define FUNCTION_SET_LINE_COUNT_2           0x08
#define FUNCTION_SET_INTERFACE_4BIT         0x00
#define FUNCTION_SET_INTERFACE_8BIT         0x10

#define DEFAULT_FUNCTION_SET (FUNCTION_SET_INTERFACE_8BIT | FUNCTION_SET_LINE_COUNT_2)

#define BIAS_1_4                         0x08
#define BIAS_1_5                         0x00
#define BIAS_FX_3_LINE                   0x01

#define ICON_DISPLAY_ON                  0x08
#define ICON_DISPLAY_OFF                 0x00
#define POWER_BOOST_ON                   0x04
#define POWER_BOOST_OFF                  0x00

#define FOLLOWER_CIRCUIT_ON              0x08
#define FOLLOWER_CIRCUIT_OFF             0x00

uint8_t st7036_ddram_row_addr[3][3] = {
    { 0x00, 0x00, 0x00 },
    { 0x00, 0x40, 0x00 },
    { 0x00, 0x10, 0x20 },
};

static int st7036_function_set(ert_driver_st7036 *driver, uint8_t instruction_set)
{
  uint8_t command = ST7036_COMMAND_FUNCTION_SET;
  int result;

  command |= DEFAULT_FUNCTION_SET;
  command |= (driver->double_height ? FUNCTION_SET_DOUBLE_HEIGHT : 0);
  command |= (instruction_set & 0x03);

  hal_gpio_pin_write(driver->static_config.pin_register_select, false);

  result = hal_spi_transfer(driver->spi_device, 1, &command);
  usleep(60 * MICROSECOND);

  return result;
}

static int st7036_write_command(ert_driver_st7036 *driver, uint8_t instruction_set, uint8_t command)
{
  int result;

  result = st7036_function_set(driver, instruction_set);
  if (result < 0) {
    return result;
  }

  hal_gpio_pin_write(driver->static_config.pin_register_select, false);

  result = hal_spi_transfer(driver->spi_device, 1, &command);
  if (result < 0) {
    return result;
  }

  usleep(60 * MICROSECOND);

  return result;
}

static int st7036_set_display_mode(ert_driver_st7036 *driver)
{
  uint8_t command = ST7036_COMMAND_SET_DISPLAY_MODE;
  command |= (driver->enabled ? DISPLAY_MODE_ENABLED : 0);
  command |= (driver->cursor_enabled ? DISPLAY_MODE_CURSOR_ENABLED : 0);
  command |= (driver->cursor_blink ? DISPLAY_MODE_CURSOR_BLINK : 0);

  return st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL, command);
}

static int st7036_set_bias(ert_driver_st7036 *driver, uint8_t bias)
{
  uint8_t command = ST7036_COMMAND_BIAS;
  command |= (bias == ERT_DRIVER_ST7036_BIAS_1_4) ? BIAS_1_4 : BIAS_1_5;
  command |= (driver->static_config.rows == 3) ? BIAS_FX_3_LINE : 0;

  return st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_EXT_1, command);
}

int st7036_set_display_enabled(ert_driver_st7036 *driver, bool enabled)
{
  driver->enabled = enabled;
  return st7036_set_display_mode(driver);
}

int st7036_set_cursor_enabled(ert_driver_st7036 *driver, bool cursor_enabled)
{
  driver->cursor_enabled = cursor_enabled;
  return st7036_set_display_mode(driver);
}

int st7036_set_cursor_blink(ert_driver_st7036 *driver, bool cursor_blink)
{
  driver->cursor_blink = cursor_blink;
  return st7036_set_display_mode(driver);
}

int st7036_reset(ert_driver_st7036 *driver)
{
  hal_gpio_pin_mode(driver->static_config.pin_reset, HAL_GPIO_PIN_MODE_OUTPUT);
  hal_gpio_pin_write(driver->static_config.pin_reset, false);
  usleep(1 * MILLISECOND);
  hal_gpio_pin_write(driver->static_config.pin_reset, true);
  usleep(1 * MILLISECOND);

  return 0;
}

int st7036_set_entry_mode(ert_driver_st7036 *driver) {
  uint8_t command = ST7036_COMMAND_SET_ENTRY_MODE;
  command |= driver->shift_cursor_right ? ENTRY_MODE_INC_DDRAM_ADDR : ENTRY_MODE_DEC_DDRAM_ADDR;
  command |= driver->shift_display ? ENTRY_MODE_SHIFT_DISPLAY : 0;

  return st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL, command);
}

int st7036_set_contrast(ert_driver_st7036 *driver, uint8_t contrast)
{
  int result;
  uint8_t command;

  // For 3.3v operation the booster must be on
  command = ST7036_COMMAND_POWER_ICON_CONTRAST;
  command |= driver->static_config.power_boost ? POWER_BOOST_ON : POWER_BOOST_OFF;
  command |= ((contrast >> 4) & 0x03);

  result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_EXT_1, command);
  if (result < 0) {
    return result;
  }

  command = ST7036_COMMAND_FOLLOWER;
  command |= driver->static_config.follower_circuit ? FOLLOWER_CIRCUIT_ON : FOLLOWER_CIRCUIT_OFF;
  command |= driver->static_config.follower_circuit_amplified_ratio & 0x07;

  result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_EXT_1, command);
  if (result < 0) {
    return result;
  }

  command = ST7036_COMMAND_CONTRAST_LSB;
  command |= (contrast & 0x0F);

  result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_EXT_1, command);
  if (result < 0) {
    return result;
  }

  return result;
}

int st7036_clear(ert_driver_st7036 *driver)
{
  int result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL, ST7036_COMMAND_CLEAR_DISPLAY);
  usleep(2 * MILLISECOND);
  return result;
}

int st7036_home(ert_driver_st7036 *driver)
{
  int result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL, ST7036_COMMAND_RETURN_HOME);
  usleep(2 * MILLISECOND);
  return result;
}

int st7036_set_cursor_position(ert_driver_st7036 *driver, uint8_t column, uint8_t row)
{
  uint8_t offset;
  int result;

  if (row > 2) {
    row = 2;
  }
  if (column > 0x3F) {
    column = 0x3F;
  }

  offset = st7036_ddram_row_addr[driver->static_config.rows - 1][row] + column;

  result = st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL,
      ST7036_COMMAND_SET_DDRAM_ADDR | offset);

  usleep(2 * 1500 * MICROSECOND);

  return result;
}

static int st7036_display_shift(ert_driver_st7036 *driver, uint8_t target, uint8_t direction)
{
  return st7036_write_command(driver, FUNCTION_SET_INSTRUCTION_SET_NORMAL,
      ST7036_COMMAND_DISPLAY_SHIFT | target | direction);
}

int st7036_shift_cursor_left(ert_driver_st7036 *driver)
{
  return st7036_display_shift(driver, DISPLAY_SHIFT_SHIFT_CURSOR, DISPLAY_SHIFT_LEFT);
}

int st7036_shift_cursor_right(ert_driver_st7036 *driver)
{
  return st7036_display_shift(driver, DISPLAY_SHIFT_SHIFT_CURSOR, DISPLAY_SHIFT_RIGHT);
}

int st7036_shift_screen_left(ert_driver_st7036 *driver)
{
  return st7036_display_shift(driver, DISPLAY_SHIFT_SHIFT_SCREEN, DISPLAY_SHIFT_LEFT);
}

int st7036_shift_screen_right(ert_driver_st7036 *driver)
{
  return st7036_display_shift(driver, DISPLAY_SHIFT_SHIFT_SCREEN, DISPLAY_SHIFT_RIGHT);
}

int st7036_write(ert_driver_st7036 *driver, char *text)
{
  int length = strlen(text);
  int i, result;
  uint8_t value;

  hal_gpio_pin_write(driver->static_config.pin_register_select, true);

  for (i = 0; i < length; i++) {
    value = text[i];
    result = hal_spi_transfer(driver->spi_device, 1, &value);
    if (result < 0) {
      return result;
    }
    usleep(2 * 50 * MICROSECOND);
  }

  return 0;
}

static int st7036_initialize(ert_driver_st7036 *driver)
{
  int result;
  result = st7036_reset(driver);

  hal_gpio_pin_mode(driver->static_config.pin_register_select, HAL_GPIO_PIN_MODE_OUTPUT);
  hal_gpio_pin_write(driver->static_config.pin_register_select, true);

  usleep(1 * MILLISECOND);

  driver->enabled = true;
  driver->cursor_enabled = false;
  driver->cursor_blink = false;
  driver->double_height = false;

  driver->shift_cursor_right = true;
  driver->shift_display = false;

  result = st7036_set_display_mode(driver);
  if (result < 0) {
    return result;
  }
  result = st7036_set_entry_mode(driver);
  if (result < 0) {
    return result;
  }

  result = st7036_set_bias(driver, driver->static_config.bias);
  if (result < 0) {
    return result;
  }
  result = st7036_set_contrast(driver, driver->static_config.initial_contrast);
  if (result < 0) {
    return result;
  }

  result = st7036_clear(driver);
  if (result < 0) {
    return result;
  }

  return 0;
}

int st7036_open(ert_driver_st7036_static_config *static_config, ert_driver_st7036 **driver_rcv)
{
  int result;
  ert_driver_st7036 *driver;

  driver = malloc(sizeof(ert_driver_st7036));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for ST7036 driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  memset(driver, 0, sizeof(ert_driver_st7036));

  result = hal_spi_open(static_config->spi_bus_index, static_config->spi_device_index,
      static_config->spi_clock_speed, 0, &driver->spi_device);
  if (result != 0) {
    free(driver);
    ert_log_error("Error opening SPI device");
    return result;
  }

  memcpy(&driver->static_config, static_config, sizeof(ert_driver_st7036_static_config));

  result = st7036_initialize(driver);
  if (result < 0) {
    free(driver);
    return result;
  }

  *driver_rcv = driver;

  return result;
}

int st7036_close(ert_driver_st7036 *driver)
{
  int result;

  result = hal_spi_close(driver->spi_device);

  free(driver);

  return result;
}
