/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "ert-hal.h"
#include "ert-hal-gpio.h"

int hal_init()
{
  return hal_gpio_init();
}

int hal_uninit()
{
  return hal_gpio_uninit();
}
