/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal-gpio-rpi.h"
#include <wiringPi.h>

int hal_gpio_rpi_init()
{
  return wiringPiSetup();
}

int hal_gpio_rpi_pin_mode(uint16_t pin, uint8_t mode)
{
  int wpi_mode;

  switch (mode) {
    case HAL_GPIO_PIN_MODE_INPUT:
      wpi_mode = INPUT;
      break;
    case HAL_GPIO_PIN_MODE_OUTPUT:
      wpi_mode = OUTPUT;
      break;
    default:
      return -1;
  }

  pinMode(pin, wpi_mode);
  return 0;
}

int hal_gpio_rpi_pin_read(uint16_t pin, bool *value)
{
  *value = (digitalRead(pin) != 0);
  return 0;
}

int hal_gpio_rpi_pin_write(uint16_t pin, bool value)
{
  digitalWrite(pin, value);
  return 0;
}

int hal_gpio_rpi_pin_isr(uint16_t pin, uint8_t edge, hal_gpio_isr function, void *data)
{
  int wpi_edge;

  switch (edge) {
    case HAL_GPIO_INT_EDGE_SETUP:
      wpi_edge = INT_EDGE_SETUP;
      break;
    case HAL_GPIO_INT_EDGE_FALLING:
      wpi_edge = INT_EDGE_FALLING;
      break;
    case HAL_GPIO_INT_EDGE_RISING:
      wpi_edge = INT_EDGE_RISING;
      break;
    case HAL_GPIO_INT_EDGE_BOTH:
      wpi_edge = INT_EDGE_BOTH;
      break;
    default:
      return -1;
  }

  wiringPiISRData(pin, wpi_edge, function, data);

  return 0;
}

int hal_gpio_rpi_uninit()
{
  return 0;
}

hal_gpio_driver hal_gpio_driver_rpi = {
    .init = hal_gpio_rpi_init,
    .uninit = hal_gpio_rpi_uninit,
    .pin_mode = hal_gpio_rpi_pin_mode,
    .pin_read = hal_gpio_rpi_pin_read,
    .pin_write = hal_gpio_rpi_pin_write,
    .pin_isr = hal_gpio_rpi_pin_isr
};
