/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal-gpio.h"
#include "ert-hal-gpio-rpi.h"

hal_gpio_driver *gpio_driver = &hal_gpio_driver_rpi;

int hal_gpio_init()
{
  return gpio_driver->init();
}

int hal_gpio_pin_mode(uint16_t pin, uint8_t mode)
{
  return gpio_driver->pin_mode(pin, mode);
}

int hal_gpio_pin_read(uint16_t pin, bool *value)
{
  return gpio_driver->pin_read(pin, value);
}

int hal_gpio_pin_write(uint16_t pin, bool value)
{
  return gpio_driver->pin_write(pin, value);
}

int hal_gpio_pin_isr(uint16_t pin, uint8_t edge, hal_gpio_isr function, void *data)
{
  return gpio_driver->pin_isr(pin, edge, function, data);
}

int hal_gpio_uninit()
{
  return gpio_driver->uninit();
}
