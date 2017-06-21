/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_HAL_GPIO_H
#define __ERT_HAL_GPIO_H

#include "ert-hal-common.h"

#define	HAL_GPIO_PIN_MODE_INPUT	  0x01
#define	HAL_GPIO_PIN_MODE_OUTPUT  0x02

#define	HAL_GPIO_INT_EDGE_SETUP		0x01
#define	HAL_GPIO_INT_EDGE_FALLING	0x02
#define	HAL_GPIO_INT_EDGE_RISING  0x03
#define	HAL_GPIO_INT_EDGE_BOTH    0x04

typedef void (*hal_gpio_isr)(void *);

int hal_gpio_init();
int hal_gpio_uninit();

int hal_gpio_pin_mode(uint16_t pin, uint8_t mode);
int hal_gpio_pin_read(uint16_t pin, bool *value);
int hal_gpio_pin_write(uint16_t pin, bool value);
int hal_gpio_pin_isr(uint16_t pin, uint8_t edge, hal_gpio_isr function, void *data);

typedef struct _hal_gpio_driver {
    int (*init)();
    int (*uninit)();
    int (*pin_mode)(uint16_t pin, uint8_t mode);
    int (*pin_read)(uint16_t pin, bool *value);
    int (*pin_write)(uint16_t pin, bool value);
    int (*pin_isr)(uint16_t pin, uint8_t edge, hal_gpio_isr function, void *data);
} hal_gpio_driver;

#endif
