/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// CAP1xxx driver code based on Pimoroni driver: https://github.com/pimoroni/cap1xxx/

#include <memory.h>
#include <errno.h>
#include <time.h>

#include "ert-driver-cap1xxx.h"
#include "ert-log.h"

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define max(X, Y) (((X) > (Y)) ? (X) : (Y))

#define CAP1XXX_I2C_ADDRESS_DEFAULT 0x28

#define CAP1XXX_REGISTER_MAIN_CONTROL 0x00
#define CAP1XXX_REGISTER_GENERAL_STATUS 0x02
#define CAP1XXX_REGISTER_INPUT_STATUS 0x03
#define CAP1XXX_REGISTER_LED_STATUS 0x04
#define CAP1XXX_REGISTER_NOISE_FLAG_STATUS 0x0A

#define CAP1XXX_REGISTER_INPUT_1_DELTA 0x10
#define CAP1XXX_REGISTER_INPUT_2_DELTA 0x11
#define CAP1XXX_REGISTER_INPUT_3_DELTA 0x12
#define CAP1XXX_REGISTER_INPUT_4_DELTA 0x13
#define CAP1XXX_REGISTER_INPUT_5_DELTA 0x14
#define CAP1XXX_REGISTER_INPUT_6_DELTA 0x15
#define CAP1XXX_REGISTER_INPUT_7_DELTA 0x16
#define CAP1XXX_REGISTER_INPUT_8_DELTA 0x17

#define CAP1XXX_REGISTER_SENSITIVITY 0x1F
// B7     = N/A
// B6..B4 = Sensitivity
// B3..B0 = Base Shift
// SENSITIVITY = {128: 0b000, 64:0b001, 32:0b010, 16:0b011, 8:0b100, 4:0b100, 2:0b110, 1:0b111}

#define CAP1XXX_REGISTER_GENERAL_CONFIG 0x20
// B7 = Timeout
// B6 = Wake Config ( 1 = Wake pin asserted )
// B5 = Disable Digital Noise ( 1 = Noise threshold disabled )
// B4 = Disable Analog Noise ( 1 = Low frequency analog noise blocking disabled )
// B3 = Max Duration Recalibration ( 1 =  Enable recalibration if touch is held longer than max duration )
// B2..B0 = N/A

#define CAP1XXX_REGISTER_INPUT_ENABLE 0x21

#define CAP1XXX_REGISTER_INPUT_CONFIG 0x22

// Default 0x00000111
#define CAP1XXX_REGISTER_INPUT_CONFIG_2 0x23

// Values for bits 3 to 0 of R_INPUT_CONFIG2
// Determines minimum amount of time before
// a "press and hold" event is detected.

// Also - Values for bits 3 to 0 of R_INPUT_CONFIG
// Determines rate at which interrupt will repeat
// Resolution of 35ms, max = 35 + (35 * 0b1111) = 560ms

// Default 0x00111001
#define CAP1XXX_REGISTER_SAMPLING_CONFIG 0x24
// Default 0b00000000
#define CAP1XXX_REGISTER_CALIBRATION 0x26
// Default 0b11111111
#define CAP1XXX_REGISTER_INTERRUPT_ENABLE 0x27
// Default 0b11111111
#define CAP1XXX_REGISTER_REPEAT_RATE_ENABLE 0x28
// Default 0b11111111
#define CAP1XXX_REGISTER_MULTIPLE_TOUCH_CONFIG 0x2A
#define CAP1XXX_REGISTER_MULTIPLE_TOUCH_PATTERN_CONFIG 0x2B
#define CAP1XXX_REGISTER_MULTIPLE_TOUCH_PATTERN 0x2D
#define CAP1XXX_REGISTER_COUNT_O_LIMIT 0x2E
#define CAP1XXX_REGISTER_RECALIBRATION_CONFIG 0x2F

#define CAP1XXX_REGISTER_INPUT_1_THRESHOLD 0x30
#define CAP1XXX_REGISTER_INPUT_2_THRESHOLD 0x31
#define CAP1XXX_REGISTER_INPUT_3_THRESHOLD 0x32
#define CAP1XXX_REGISTER_INPUT_4_THRESHOLD 0x33
#define CAP1XXX_REGISTER_INPUT_5_THRESHOLD 0x34
#define CAP1XXX_REGISTER_INPUT_6_THRESHOLD 0x35
#define CAP1XXX_REGISTER_INPUT_7_THRESHOLD 0x36
#define CAP1XXX_REGISTER_INPUT_8_THRESHOLD 0x37

#define CAP1XXX_REGISTER_NOISE_THRESHOLD 0x38

#define CAP1XXX_REGISTER_STANDBY_CHANNEL 0x40
#define CAP1XXX_REGISTER_STANDBY_CONFIG 0x41
#define CAP1XXX_REGISTER_STANDBY_SENSITIVITY 0x42
#define CAP1XXX_REGISTER_STANDBY_THRESHOLD 0x43

#define CAP1XXX_REGISTER_CONFIGURATION_2 0x44
// B7 = Linked LED Transition Controls ( 1 = LED trigger is !touch )
// B6 = Alert Polarity ( 1 = Active Low Open Drain, 0 = Active High Push Pull )
// B5 = Reduce Power ( 1 = Do not power down between poll )
// B4 = Link Polarity/Mirror bits ( 0 = Linked, 1 = Unlinked )
// B3 = Show RF Noise ( 1 = Noise status registers only show RF, 0 = Both RF and EMI shown )
// B2 = Disable RF Noise ( 1 = Disable RF noise filter )
// B1..B0 = N/A

#define CAP1XXX_REGISTER_INPUT_1_BASE_COUNT 0x50
#define CAP1XXX_REGISTER_INPUT_2_BASE_COUNT 0x51
#define CAP1XXX_REGISTER_INPUT_3_BASE_COUNT 0x52
#define CAP1XXX_REGISTER_INPUT_4_BASE_COUNT 0x53
#define CAP1XXX_REGISTER_INPUT_5_BASE_COUNT 0x54
#define CAP1XXX_REGISTER_INPUT_6_BASE_COUNT 0x55
#define CAP1XXX_REGISTER_INPUT_7_BASE_COUNT 0x56
#define CAP1XXX_REGISTER_INPUT_8_BASE_COUNT 0x57

#define CAP1XXX_REGISTER_LED_OUTPUT_TYPE 0x71
#define CAP1XXX_REGISTER_LED_LINKING 0x72
#define CAP1XXX_REGISTER_LED_POLARITY 0x73
#define CAP1XXX_REGISTER_LED_OUTPUT_CONTROL 0x74
#define CAP1XXX_REGISTER_LED_LINKED_TRANSITION_CONTROL 0x77
#define CAP1XXX_REGISTER_LED_MIRROR_CONTROL 0x79

// For LEDs 1-4
#define CAP1XXX_REGISTER_LED_BEHAVIOUR_1 0x81
// For LEDs 5-8
#define CAP1XXX_REGISTER_LED_BEHAVIOUR_2 0x82
#define CAP1XXX_REGISTER_LED_PULSE_1_PERIOD 0x84
#define CAP1XXX_REGISTER_LED_PULSE_2_PERIOD 0x85
#define CAP1XXX_REGISTER_LED_BREATHE_PERIOD 0x86
#define CAP1XXX_REGISTER_LED_CONFIG 0x88
#define CAP1XXX_REGISTER_LED_PULSE_1_DUTY_CYCLE 0x90
#define CAP1XXX_REGISTER_LED_PULSE_2_DUTY_CYCLE 0x91
#define CAP1XXX_REGISTER_LED_BREATHE_DUTY_CYCLE 0x92
#define CAP1XXX_REGISTER_LED_DIRECT_DUTY_CYCLE 0x93
#define CAP1XXX_REGISTER_LED_DIRECT_RAMP_RATES 0x94
#define CAP1XXX_REGISTER_LED_OFF_DELAY 0x95

#define CAP1XXX_REGISTER_POWER_BUTTON 0x60
#define CAP1XXX_REGISTER_POWER_BUTTON_CONFIG 0x61

#define CAP1XXX_REGISTER_INPUT_1_CALIBRATION 0xB1
#define CAP1XXX_REGISTER_INPUT_2_CALIBRATION 0xB2
#define CAP1XXX_REGISTER_INPUT_3_CALIBRATION 0xB3
#define CAP1XXX_REGISTER_INPUT_4_CALIBRATION 0xB4
#define CAP1XXX_REGISTER_INPUT_5_CALIBRATION 0xB5
#define CAP1XXX_REGISTER_INPUT_6_CALIBRATION 0xB6
#define CAP1XXX_REGISTER_INPUT_7_CALIBRATION 0xB7
#define CAP1XXX_REGISTER_INPUT_8_CALIBRATION 0xB8

#define CAP1XXX_REGISTER_INPUT_CALIBRATION_LSB_1 0xB9
#define CAP1XXX_REGISTER_INPUT_CALIBRATION_LSB_2 0xBA

#define CAP1XXX_REGISTER_PRODUCT_ID 0xFD
#define CAP1XXX_REGISTER_MANUFACTURER_ID 0xFE
#define CAP1XXX_REGISTER_REVISION 0xFF

// Default, LED is open-drain output with ext pullup
#define CAP1XXX_REGISTER_LED_OPEN_DRAIN 0
// LED is driven HIGH/LOW with logic 1/0
#define CAP1XXX_REGISTER_LED_PUSH_PULL 1

#define CAP1XXX_REGISTER_LED_RAMP_RATE_2000MS 7
#define CAP1XXX_REGISTER_LED_RAMP_RATE_1500MS 6
#define CAP1XXX_REGISTER_LED_RAMP_RATE_1250MS 5
#define CAP1XXX_REGISTER_LED_RAMP_RATE_1000MS 4
#define CAP1XXX_REGISTER_LED_RAMP_RATE_750MS 3
#define CAP1XXX_REGISTER_LED_RAMP_RATE_500MS 2
#define CAP1XXX_REGISTER_LED_RAMP_RATE_250MS 1
#define CAP1XXX_REGISTER_LED_RAMP_RATE_0MS 0

int cap1xxx_enable_inputs(ert_driver_cap1xxx *driver, uint8_t inputs)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_ENABLE, inputs);
}

int cap1xxx_enable_interrupts(ert_driver_cap1xxx *driver, uint8_t inputs)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INTERRUPT_ENABLE, inputs);
}

int cap1xxx_enable_repeat(ert_driver_cap1xxx *driver, uint8_t inputs)
{
  driver->repeat_enabled = inputs;
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_REPEAT_RATE_ENABLE, inputs);
}

int cap1xxx_enable_multitouch(ert_driver_cap1xxx *driver, bool enable)
{
  uint8_t value;

  int result = hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_MULTIPLE_TOUCH_CONFIG, &value);
  if (result < 0) {
    return result;
  }

  if (enable) {
    // Disable blocking of multiple touches
    value &= ~0x80;
  } else {
    value |= 0x80;
  }

  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_MULTIPLE_TOUCH_CONFIG, value);
}

int cap1xxx_get_product_id(ert_driver_cap1xxx *driver, uint8_t *product_id_rcv)
{
  return hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_PRODUCT_ID, product_id_rcv);
}

static uint16_t cap1xxx_calculate_touch_rate(uint16_t milliseconds)
{
  uint16_t clamped = (uint16_t) min(max(milliseconds, 0), 560);
  uint16_t scaled = (uint16_t) ((((uint32_t) ((float) clamped / 35.0f) * 35.0f) - 35) / 35);
  return scaled;
}

int cap1xxx_set_repeat_rate(ert_driver_cap1xxx *driver, uint16_t milliseconds)
{
  uint8_t value;

  int result = hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_CONFIG, &value);
  if (result < 0) {
    return result;
  }

  value = (uint8_t) ((value & 0xF0) | cap1xxx_calculate_touch_rate(milliseconds));

  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_CONFIG, value);
}

int cap1xxx_set_hold_delay(ert_driver_cap1xxx *driver, uint16_t milliseconds)
{
  uint8_t value;

  int result = hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_CONFIG_2, &value);
  if (result < 0) {
    return result;
  }

  value = (uint8_t) ((value & 0xF0) | cap1xxx_calculate_touch_rate(milliseconds));

  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_CONFIG_2, value);
}

static int cap1xxx_set_register_bit(ert_driver_cap1xxx *driver, uint8_t reg, uint8_t bit, bool bit_value)
{
  uint8_t reg_value;
  int result = hal_i2c_read_byte_command(driver->i2c_device, reg, &reg_value);
  if (result < 0) {
    return result;
  }

  if (bit_value) {
    reg_value |= (1 << bit);
  } else {
    reg_value &= ~(1 << bit);
  }

  return hal_i2c_write_byte_command(driver->i2c_device, reg, reg_value);
}

static int cap1xxx_set_register_bits(ert_driver_cap1xxx *driver, uint8_t reg, uint8_t offset, uint8_t size, uint8_t value)
{
  uint8_t reg_value;
  int result = hal_i2c_read_byte_command(driver->i2c_device, reg, &reg_value);
  if (result < 0) {
    return result;
  }

  for (uint8_t i = offset; i < (offset + size); i++) {
    reg_value &= ~(1 << i);
  }

  reg_value |= (value << offset);

  return hal_i2c_write_byte_command(driver->i2c_device, reg, reg_value);
}

int cap1xxx_auto_recalibrate(ert_driver_cap1xxx *driver, bool enable)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_GENERAL_CONFIG, 3, enable);
}

int cap1xxx_filter_analog_noise(ert_driver_cap1xxx *driver, bool enable)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_GENERAL_CONFIG, 4, !enable);
}

int cap1xxx_filter_digital_noise(ert_driver_cap1xxx *driver, bool enable)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_GENERAL_CONFIG, 5, !enable);
}

static int cap1xxx_clear_interrupt(ert_driver_cap1xxx *driver)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_MAIN_CONTROL, 0, false);
}

static int cap1xxx_get_interrupt_status(ert_driver_cap1xxx *driver, bool *interrupt_status)
{
  uint8_t value;

  int result = hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_MAIN_CONTROL, &value);
  if (result < 0) {
    return result;
  }

  *interrupt_status = (value & 0x01) ? true : false;

  return 0;
}

static int cap1xxx_wait_for_interrupt(ert_driver_cap1xxx *driver, uint32_t timeout_millis, bool *interrupt_status)
{
  int result;

  struct timespec ts;

  result = clock_gettime(CLOCK_REALTIME, &ts);
  if (result < 0) {
    return -EIO;
  }

  time_t start_millis = ts.tv_sec * 1000L + (ts.tv_nsec / 1000000L);
  time_t now_millis;

  do {
    result = cap1xxx_get_interrupt_status(driver, interrupt_status);
    if (result < 0) {
      return result;
    }

    if (*interrupt_status) {
      return 0;
    }

    result = clock_gettime(CLOCK_REALTIME, &ts);
    if (result < 0) {
      return -EIO;
    }

    now_millis = ts.tv_sec * 1000L + (ts.tv_nsec / 1000000L);
  } while ((now_millis - start_millis) < timeout_millis);

  return 0;
}

int cap1xxx_get_input_state(ert_driver_cap1xxx *driver, ert_driver_cap1xxx_input_state *input_state_rcv)
{
  uint8_t input_touched;
  uint8_t threshold_values[CAP1XXX_INPUT_COUNT_MAX];

  int result = hal_i2c_read_byte_command(driver->i2c_device, CAP1XXX_REGISTER_INPUT_STATUS, &input_touched);
  if (result < 0) {
    return result;
  }

  result = hal_i2c_read_i2c_block(driver->i2c_device, CAP1XXX_REGISTER_INPUT_1_THRESHOLD,
      driver->input_count, (uint8_t *) &threshold_values, NULL);
  if (result < 0) {
    return result;
  }

  result = hal_i2c_read_i2c_block(driver->i2c_device, CAP1XXX_REGISTER_INPUT_1_DELTA,
      driver->input_count, (uint8_t *) driver->input_delta, NULL);
  if (result < 0) {
    return result;
  }

  cap1xxx_clear_interrupt(driver);

  for (uint8_t i = 0; i < driver->input_count; i++) {
    ert_driver_cap1xxx_input_state state = CAP1XXX_INPUT_STATE_NONE;
    bool touched = (input_touched & (1 << i)) != 0;

    if (touched) {
      ert_driver_cap1xxx_input_state previous_state = driver->input_state[i];

      if (driver->input_delta[i] > threshold_values[i]) {
        if (previous_state == CAP1XXX_INPUT_STATE_PRESSED
            || previous_state == CAP1XXX_INPUT_STATE_HELD) {
          if ((driver->repeat_enabled & (1 << i)) != 0) {
            state = CAP1XXX_INPUT_STATE_HELD;
          } else {
            state = CAP1XXX_INPUT_STATE_NONE;
          }
        } else {
          if (driver->input_pressed[i]) {
            state = CAP1XXX_INPUT_STATE_NONE;
          } else {
            state = CAP1XXX_INPUT_STATE_PRESSED;
          }
        }
      } else {
        if (driver->input_state[i] == CAP1XXX_INPUT_STATE_RELEASED) {
          state = CAP1XXX_INPUT_STATE_NONE;
        } else {
          state = CAP1XXX_INPUT_STATE_RELEASED;
        }
      }

      driver->input_state[i] = state;
      driver->input_pressed[i] =
          (state == CAP1XXX_INPUT_STATE_PRESSED
           || state == CAP1XXX_INPUT_STATE_HELD
           || state == CAP1XXX_INPUT_STATE_RELEASED) ? true : false;
    } else {
      driver->input_state[i] = CAP1XXX_INPUT_STATE_NONE;
      driver->input_pressed[i] = false;
    }
  }

  memcpy(input_state_rcv, driver->input_state, driver->input_count * sizeof(ert_driver_cap1xxx_input_state));

  return 0;
}

uint8_t cap1xxx_get_input_count(ert_driver_cap1xxx *driver)
{
  return driver->input_count;
}

uint8_t cap1xxx_get_led_count(ert_driver_cap1xxx *driver)
{
  return driver->led_count;
}

int cap1xxx_wait_for_input(ert_driver_cap1xxx *driver, uint32_t timeout_millis,
    ert_driver_cap1xxx_input_state *input_state_rcv)
{
  bool interrupt_status;
  int result = cap1xxx_wait_for_interrupt(driver, timeout_millis, &interrupt_status);
  if (result < 0) {
    return result;
  }

  if (!interrupt_status) {
    for (uint8_t i = 0; i < driver->input_count; i++) {
      input_state_rcv[i] = CAP1XXX_INPUT_STATE_NONE;
    }
    return 0;
  }

  result = cap1xxx_get_input_state(driver, input_state_rcv);

  return result;
}

int cap1xxx_set_led_linking(ert_driver_cap1xxx *driver, uint8_t led_index, bool enable)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_LED_LINKING, led_index, enable);
}

int cap1xxx_set_led_output_type(ert_driver_cap1xxx *driver, uint8_t led_index, bool push_pull)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_LED_OUTPUT_TYPE, led_index, push_pull);
}

int cap1xxx_set_led_state(ert_driver_cap1xxx *driver, uint8_t led_index, bool on)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_LED_OUTPUT_CONTROL, led_index, on);
}

int cap1xxx_set_led_state_all(ert_driver_cap1xxx *driver, uint8_t led_state_mask)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_OUTPUT_CONTROL, led_state_mask);
}

int cap1xxx_set_led_polarity(ert_driver_cap1xxx *driver, uint8_t led_index, bool non_inverted)
{
  return cap1xxx_set_register_bit(driver, CAP1XXX_REGISTER_LED_POLARITY, led_index, non_inverted);
}

int cap1xxx_set_led_behavior(ert_driver_cap1xxx *driver, uint8_t led_index, uint8_t behavior)
{
  uint8_t offset = (uint8_t) ((led_index * 2) % 8);
  uint8_t register_offset = (uint8_t) (led_index / 4);

  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_BEHAVIOUR_1 + register_offset, offset, 2, behavior & 0x03);
}

int cap1xxx_set_led_direct_ramp_rate(ert_driver_cap1xxx *driver, uint32_t rise_millis, uint32_t fall_millis)
{
  uint8_t rise = (uint8_t) (rise_millis / 250);
  uint8_t fall = (uint8_t) (fall_millis / 250);

  rise = (rise & 0x07);
  fall = (fall & 0x07);

  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_DIRECT_RAMP_RATES,
      (rise << 4) | fall);
}

int cap1xxx_set_led_direct_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_DIRECT_DUTY_CYCLE,
      (duty_max << 4) | duty_min);
}

int cap1xxx_set_led_direct_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_DIRECT_DUTY_CYCLE,
      0, 4, duty_min);
}

int cap1xxx_set_led_direct_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_DIRECT_DUTY_CYCLE,
      4, 4, duty_max);
}

static uint8_t cap1xxx_calculate_pulse_period(uint32_t milliseconds)
{
  float capped_milliseconds = min(milliseconds, 4064.0f);
  return (uint8_t) (((uint32_t) (capped_milliseconds / 32.0f)) & 0x7F);
}

int cap1xxx_set_led_pulse1_period(ert_driver_cap1xxx *driver, uint32_t milliseconds)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_1_PERIOD, 0, 7,
    cap1xxx_calculate_pulse_period(milliseconds));
}

int cap1xxx_set_led_pulse2_period(ert_driver_cap1xxx *driver, uint32_t milliseconds)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_2_PERIOD, 0, 7,
      cap1xxx_calculate_pulse_period(milliseconds));
}

int cap1xxx_set_led_breathe_period(ert_driver_cap1xxx *driver, uint32_t milliseconds)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_BREATHE_PERIOD, 0, 7,
      cap1xxx_calculate_pulse_period(milliseconds));
}

int cap1xxx_set_led_pulse1_count(ert_driver_cap1xxx *driver, uint8_t count)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_CONFIG, 0, 3,
      (uint8_t) ((count - 1) & 0x07));
}

int cap1xxx_set_led_pulse2_count(ert_driver_cap1xxx *driver, uint8_t count)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_CONFIG, 3, 3,
      (uint8_t) ((count - 1) & 0x07));
}

int cap1xxx_set_led_pulse1_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_PULSE_1_DUTY_CYCLE,
      (duty_max << 4) | duty_min);
}

int cap1xxx_set_led_pulse1_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_1_DUTY_CYCLE,
      0, 4, duty_min);
}

int cap1xxx_set_led_pulse1_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_1_DUTY_CYCLE,
      4, 4, duty_max);
}

int cap1xxx_set_led_pulse2_duty(ert_driver_cap1xxx *driver, uint8_t duty_min, uint8_t duty_max)
{
  return hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_PULSE_2_DUTY_CYCLE,
      (duty_max << 4) | duty_min);
}

int cap1xxx_set_led_pulse2_duty_min(ert_driver_cap1xxx *driver, uint8_t duty_min)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_2_DUTY_CYCLE,
      0, 4, duty_min);
}

int cap1xxx_set_led_pulse2_duty_max(ert_driver_cap1xxx *driver, uint8_t duty_max)
{
  return cap1xxx_set_register_bits(driver, CAP1XXX_REGISTER_LED_PULSE_2_DUTY_CYCLE,
      4, 4, duty_max);
}

int cap1xxx_open(int bus, uint8_t i2c_address, ert_driver_cap1xxx **driver_rcv)
{
  int result;
  ert_driver_cap1xxx *driver;

  driver = calloc(1, sizeof(ert_driver_cap1xxx));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for CAP1xxx driver struct: %s", strerror(errno));
    return -ENOMEM;
  }

  result = hal_i2c_open(bus, i2c_address, &driver->i2c_device);
  if (result != 0) {
    free(driver);
    ert_log_error("Error opening I2C device");
    result = -EIO;
    goto error_driver;
  }

  *driver_rcv = driver;

  uint8_t product_id;
  result = cap1xxx_get_product_id(driver, &product_id);
  if (result < 0) {
    ert_log_error("cap1xxx_get_product_id failed with result %d", result);
    goto error_i2c;
  }

  switch (product_id) {
    case CAP1XXX_PRODUCT_ID_CAP1166:
      driver->input_count = 6;
      driver->led_count = 6;
      break;
    case CAP1XXX_PRODUCT_ID_CAP1188:
      driver->input_count = 8;
      driver->led_count = 8;
      break;
    case CAP1XXX_PRODUCT_ID_CAP1208:
      driver->input_count = 8;
      driver->led_count = 0;
      break;
    default:
      ert_log_error("Unknown CAP1XXX product ID: 0x%02X", product_id);
      result = -EIO;
      goto error_i2c;
  }

  result = cap1xxx_enable_inputs(driver, 0xFF);
  if (result < 0) {
    ert_log_error("cap1xxx_enable_inputs failed with result %d", result);
    goto error_i2c;
  }
  result = cap1xxx_enable_interrupts(driver, 0xFF);
  if (result < 0) {
    ert_log_error("cap1xxx_enable_interrupts failed with result %d", result);
    goto error_i2c;
  }
  result = cap1xxx_enable_repeat(driver, 0xFF);
  if (result < 0) {
    ert_log_error("cap1xxx_enable_repeat failed with result %d", result);
    goto error_i2c;
  }
  result = cap1xxx_enable_multitouch(driver, true);
  if (result < 0) {
    ert_log_error("cap1xxx_enable_multitouch failed with result %d", result);
    goto error_i2c;
  }
  result = cap1xxx_set_hold_delay(driver, 210);
  if (result < 0) {
    ert_log_error("cap1xxx_set_hold_delay failed with result %d", result);
    goto error_i2c;
  }
  result = cap1xxx_set_repeat_rate(driver, 210);
  if (result < 0) {
    ert_log_error("cap1xxx_set_repeat_rate failed with result %d", result);
    goto error_i2c;
  }

  result = hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_LED_LINKING, 0x00);
  if (result < 0) {
    ert_log_error("hal_i2c_write_byte_command failed with result %d", result);
    goto error_i2c;
  }
  result = hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_SAMPLING_CONFIG, 0x08);
  if (result < 0) {
    ert_log_error("hal_i2c_write_byte_command failed with result %d", result);
    goto error_i2c;
  }
  result = hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_SENSITIVITY, 0x60);
  if (result < 0) {
    ert_log_error("hal_i2c_write_byte_command failed with result %d", result);
    goto error_i2c;
  }
  result = hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_GENERAL_CONFIG, 0x38);
  if (result < 0) {
    ert_log_error("hal_i2c_write_byte_command failed with result %d", result);
    goto error_i2c;
  }
  result = hal_i2c_write_byte_command(driver->i2c_device, CAP1XXX_REGISTER_CONFIGURATION_2, 0x60);
  if (result < 0) {
    ert_log_error("hal_i2c_write_byte_command failed with result %d", result);
    goto error_i2c;
  }

  return 0;

  error_i2c:
  hal_i2c_close(driver->i2c_device);

  error_driver:
  free(driver);

  return result;
}

int cap1xxx_close(ert_driver_cap1xxx *driver)
{
  int result;

  result = hal_i2c_close(driver->i2c_device);

  free(driver);

  return result;
}
