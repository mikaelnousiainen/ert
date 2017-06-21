/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_CAP1XXX_H
#define __ERT_DRIVER_CAP1XXX_H

#include "ert-hal-i2c.h"

#define CAP1XXX_INPUT_COUNT_MAX 8

#define CAP1XXX_PRODUCT_ID_CAP1166 0x51
#define CAP1XXX_PRODUCT_ID_CAP1188 0x50
#define CAP1XXX_PRODUCT_ID_CAP1208 0x6B

#define CAP1XXX_REGISTER_LED_BEHAVIOUR_DIRECT 0x00
#define CAP1XXX_REGISTER_LED_BEHAVIOUR_PULSE1 0x01
#define CAP1XXX_REGISTER_LED_BEHAVIOUR_PULSE2 0x02
#define CAP1XXX_REGISTER_LED_BEHAVIOUR_BREATHE 0x03

#define CAP1XXX_LED_DUTY_MIN 0
#define CAP1XXX_LED_DUTY_MAX 15

typedef enum _ert_driver_cap1xxx_input_state {
  CAP1XXX_INPUT_STATE_NONE = 0,
  CAP1XXX_INPUT_STATE_PRESSED = 1,
  CAP1XXX_INPUT_STATE_HELD = 2,
  CAP1XXX_INPUT_STATE_RELEASED = 3,
} ert_driver_cap1xxx_input_state;

typedef struct _ert_driver_cap1xxx {
  hal_i2c_device *i2c_device;
  uint8_t input_count;
  uint8_t led_count;

  uint8_t repeat_enabled;

  ert_driver_cap1xxx_input_state input_state[CAP1XXX_INPUT_COUNT_MAX];
  bool input_pressed[CAP1XXX_INPUT_COUNT_MAX];
  int8_t input_delta[CAP1XXX_INPUT_COUNT_MAX];
} ert_driver_cap1xxx;

int cap1xxx_enable_inputs(ert_driver_cap1xxx *driver, uint8_t inputs);
int cap1xxx_enable_interrupts(ert_driver_cap1xxx *driver, uint8_t inputs);
int cap1xxx_enable_repeat(ert_driver_cap1xxx *driver, uint8_t inputs);
int cap1xxx_enable_multitouch(ert_driver_cap1xxx *driver, bool enable);

int cap1xxx_get_product_id(ert_driver_cap1xxx *driver, uint8_t *product_id_rcv);
int cap1xxx_set_repeat_rate(ert_driver_cap1xxx *driver, uint16_t milliseconds);
int cap1xxx_set_hold_delay(ert_driver_cap1xxx *driver, uint16_t milliseconds);

int cap1xxx_auto_recalibrate(ert_driver_cap1xxx *driver, bool enable);
int cap1xxx_filter_analog_noise(ert_driver_cap1xxx *driver, bool enable);
int cap1xxx_filter_digital_noise(ert_driver_cap1xxx *driver, bool enable);

int cap1xxx_get_input_state(ert_driver_cap1xxx *driver, ert_driver_cap1xxx_input_state *input_state_rcv);

uint8_t cap1xxx_get_input_count(ert_driver_cap1xxx *driver);
uint8_t cap1xxx_get_led_count(ert_driver_cap1xxx *driver);

int cap1xxx_wait_for_input(ert_driver_cap1xxx *driver, uint32_t timeout_millis,
    ert_driver_cap1xxx_input_state *input_state_rcv);

int cap1xxx_set_led_linking(ert_driver_cap1xxx *driver, uint8_t led_index, bool enable);
int cap1xxx_set_led_output_type(ert_driver_cap1xxx *driver, uint8_t led_index, bool push_pull);
int cap1xxx_set_led_state(ert_driver_cap1xxx *driver, uint8_t led_index, bool on);
int cap1xxx_set_led_state_all(ert_driver_cap1xxx *driver, uint8_t led_state_mask);
int cap1xxx_set_led_polarity(ert_driver_cap1xxx *driver, uint8_t led_index, bool non_inverted);
int cap1xxx_set_led_behavior(ert_driver_cap1xxx *driver, uint8_t led_index, uint8_t behavior);

int cap1xxx_set_led_direct_ramp_rate(ert_driver_cap1xxx *driver, uint32_t rise_millis, uint32_t fall_millis);
int cap1xxx_set_led_direct_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max);
int cap1xxx_set_led_direct_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min);
int cap1xxx_set_led_direct_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max);

int cap1xxx_set_led_pulse1_period(ert_driver_cap1xxx *driver, uint32_t milliseconds);
int cap1xxx_set_led_pulse2_period(ert_driver_cap1xxx *driver, uint32_t milliseconds);
int cap1xxx_set_led_breathe_period(ert_driver_cap1xxx *driver, uint32_t milliseconds);
int cap1xxx_set_led_pulse1_count(ert_driver_cap1xxx *driver, uint8_t count);
int cap1xxx_set_led_pulse2_count(ert_driver_cap1xxx *driver, uint8_t count);
int cap1xxx_set_led_pulse1_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max);
int cap1xxx_set_led_pulse1_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min);
int cap1xxx_set_led_pulse1_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max);
int cap1xxx_set_led_pulse2_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max);
int cap1xxx_set_led_pulse2_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min);
int cap1xxx_set_led_pulse2_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max);

int cap1xxx_open(int bus, uint8_t i2c_address, ert_driver_cap1xxx **driver_rcv);
int cap1xxx_close(ert_driver_cap1xxx *driver);

#endif
