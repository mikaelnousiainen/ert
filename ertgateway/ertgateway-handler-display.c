/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <ert-sensor.h>

#include "ertgateway.h"

#include "ert-driver-dothat-backlight.h"
#include "ert-driver-dothat-touch.h"
#include "ert-driver-dothat-led.h"

#define RSSI_MIN -100

#define ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE 256

typedef enum _ert_gateway_handler_display_mode {
  DISPLAY_MODE_COMM_DEVICE = 0,
  DISPLAY_MODE_NODE_POSITION = 1,
  DISPLAY_MODE_NODE_SENSORS = 2,
  DISPLAY_MODE_GATEWAY_POSITION = 3,
  DISPLAY_MODE_LAST = DISPLAY_MODE_NODE_SENSORS,
  DISPLAY_MODE_LAST_WITH_GPS = DISPLAY_MODE_GATEWAY_POSITION,
} ert_gateway_handler_display_mode;

typedef struct _ert_gateway_handler_display_context {
  ert_gateway *gateway;

  ert_driver_st7036 *display;
  ert_driver_dothat_backlight *backlight;
  ert_driver_cap1xxx *cap1xxx;
  ert_driver_dothat_touch *dothat_touch;
  ert_driver_dothat_led *dothat_led;

  uint64_t last_invalid_receive_packet_count;
  float brightness;

  char node_position_text1[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char node_position_text2[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char node_position_text3[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];

  char node_sensors_text1[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char node_sensors_text2[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char node_sensors_text3[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];

  char gateway_position_text1[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char gateway_position_text2[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];
  char gateway_position_text3[ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE];

  ert_gateway_handler_display_mode display_mode;

  uint32_t display_update_counter;
} ert_gateway_handler_display_context;

void ert_gateway_handler_display_dothat_initialize(ert_gateway_handler_display_context *display_context)
{
  int result;

  display_context->brightness = ((float) display_context->gateway->config.dothat_config.brightness / 100.0f);

  bool dothat_backlight_enabled = display_context->gateway->config.dothat_config.backlight_enabled;

  if (dothat_backlight_enabled) {
    result = dothat_backlight_open(1, &display_context->backlight);
    if (result < 0) {
      ert_log_error("dothat_backlight_open failed with result %d", result);
      display_context->backlight = NULL;
    } else {
      dothat_backlight_off(display_context->backlight);
    }
  }

  bool dothat_touch_enabled = display_context->gateway->config.dothat_config.touch_enabled;
  bool dothat_leds_enabled = display_context->gateway->config.dothat_config.leds_enabled;

  if (dothat_touch_enabled || dothat_leds_enabled) {
    result = cap1xxx_open(1, DOTHAT_TOUCH_I2C_ADDRESS, &display_context->cap1xxx);
    if (result < 0) {
      ert_log_error("cap1xxx_open failed with result %d", result);
      display_context->cap1xxx = NULL;
    }
  }

  if (display_context->cap1xxx != NULL) {
    if (dothat_touch_enabled) {
      result = dothat_touch_open(display_context->cap1xxx, &display_context->dothat_touch);
      if (result < 0) {
        ert_log_error("dothat_touch_open failed with result %d", result);
        display_context->dothat_touch = NULL;
      } else {
        dothat_touch_set_repeat_state(display_context->dothat_touch, false);
      }
    }

    if (dothat_leds_enabled) {
      result = dothat_led_open(display_context->cap1xxx, &display_context->dothat_led);
      if (result < 0) {
        ert_log_error("dothat_led_open failed with result %d", result);
        display_context->dothat_led = NULL;
      } else {
        float led_brightness = (display_context->brightness - 0.6f) * 2;
        if (led_brightness < 0) {
          led_brightness = 0;
        }
        dothat_led_off(display_context->dothat_led);
        dothat_led_set_brightness(display_context->dothat_led, (uint8_t) (DOTHAT_LED_BRIGHTNESS_MAX * led_brightness));
        dothat_led_set_ramp_rate(display_context->dothat_led, 250);
      }
    }
  }
}

void ert_gateway_handler_display_dothat_uninitialize(ert_gateway_handler_display_context *display_context)
{
  if (display_context->backlight != NULL) {
    dothat_backlight_off(display_context->backlight);
    dothat_backlight_close(display_context->backlight);
  }

  if (display_context->dothat_led != NULL) {
    dothat_led_off(display_context->dothat_led);
    dothat_led_close(display_context->dothat_led);
  }

  if (display_context->dothat_touch != NULL) {
    dothat_touch_close(display_context->dothat_touch);
  }

  if (display_context->cap1xxx != NULL) {
    cap1xxx_close(display_context->cap1xxx);
  }
}

int ert_gateway_handler_display_context_initialize(ert_gateway *gateway, ert_gateway_handler_display_context *display_context)
{
  int result;

  result = st7036_open(&gateway->config.st7036_static_config, &display_context->display);
  if (result != 0) {
    ert_log_error("st7036_open failed with result: %d", result);
    return -EIO;
  }

  display_context->gateway = gateway;
  display_context->last_invalid_receive_packet_count = 0;

  display_context->display_mode = DISPLAY_MODE_COMM_DEVICE;

  ert_gateway_handler_display_dothat_initialize(display_context);

  return 0;
}

int ert_gateway_handler_display_context_uninitialize(ert_gateway_handler_display_context *display_context)
{
  ert_gateway_handler_display_dothat_uninitialize(display_context);

  st7036_clear(display_context->display);
  st7036_set_contrast(display_context->display, 0);
  st7036_set_display_enabled(display_context->display, false);
  st7036_close(display_context->display);

  return 0;
}

void ert_gateway_handler_display_create_comm_device_status_text(ert_comm_device_status *status, size_t length, char *text)
{
  ert_driver_rfm9xw_status *rfm9xw_status = (ert_driver_rfm9xw_status *) status->custom;

  snprintf(text, length, "S%04.0fP%04.0f %c%c%c%c%c",
      status->current_rssi,
      status->last_received_packet_rssi,
      rfm9xw_status->signal_detected ? 'D' : ' ',
      rfm9xw_status->signal_synchronized ? 'S' : ' ',
      rfm9xw_status->rx_active ? 'R' : ' ',
      rfm9xw_status->header_info_valid ? 'H' : ' ',
      rfm9xw_status->modem_clear ? 'C' : ' ');
}

void ert_gateway_handler_display_create_comm_device_counter_text(ert_comm_device_status *status, size_t length, char *text)
{
  snprintf(text, length, "R%04" PRIu64 "T%04" PRIu64 "E%04" PRIu64,
    status->received_packet_count % 10000,
    status->transmitted_packet_count % 10000,
    status->invalid_received_packet_count % 10000);
}

void ert_gateway_handler_display_create_packet_time_text(ert_comm_device_status *status, size_t length, char *text)
{
  int32_t seconds_since_last_received_packet = (status->last_received_packet_timestamp.tv_sec == 0LL)
      ? -1 : ert_timespec_diff_milliseconds_from_current(&status->last_received_packet_timestamp) / 1000;
  int32_t seconds_since_last_transmitted_packet = (status->last_transmitted_packet_timestamp.tv_sec == 0LL)
      ? -1 : ert_timespec_diff_milliseconds_from_current(&status->last_transmitted_packet_timestamp) / 1000;

  snprintf(text, length, "SR%04dST%04d",
      seconds_since_last_received_packet % 10000,
      seconds_since_last_transmitted_packet % 10000);
}

void ert_gateway_handler_display_create_position_text(ert_data_logger_entry *entry, size_t length,
    char *text1, char *text2, char *text3)
{
  snprintf(text1, length, "SV %02d SU %02d",
      entry->params->gps_data.satellites_visible,
      entry->params->gps_data.satellites_used);

  snprintf(text2, length, "%07.3fN%07.3fE",
      entry->params->gps_data.latitude_degrees,
      entry->params->gps_data.longitude_degrees);

  snprintf(text3, length, "%05dm %04.1f m/s",
      (int32_t) entry->params->gps_data.altitude_meters,
      entry->params->gps_data.speed_meters_per_sec);
}

ert_sensor_data *ert_gateway_handler_display_get_sensor_data(ert_data_logger_entry *entry, ert_sensor_data_type data_type) {
  for (size_t module_index = 0; module_index < entry->params->sensor_module_data_count; module_index++) {
    ert_sensor_module_data *module_data = entry->params->sensor_module_data[module_index];

    for (size_t sensor_index = 0; sensor_index < module_data->module->sensor_count; sensor_index++) {
      ert_sensor_with_data *sensor_with_data = &module_data->sensor_data_array[sensor_index];

      if (!sensor_with_data->available) {
        continue;
      }

      for (size_t data_type_index = 0; data_type_index < sensor_with_data->sensor->data_type_count; data_type_index++) {
        if (sensor_with_data->sensor->data_types[data_type_index] == data_type) {
          return &sensor_with_data->data_array[data_type_index];
        }
      }
    }
  }

  return NULL;
}

int ert_gateway_handler_display_format_sensor_value(ert_sensor_data *data, char *label, char *value_format, size_t length, char *text)
{
  size_t format_length = 128;
  char format[format_length];

  if (data == NULL || !data->available) {
    return snprintf(text, length, "%s - ", label);
  }

  snprintf(format, format_length, "%%s%s%%s ", value_format);

  return snprintf(text, length, format, label, data->value.value, data->unit);
}

void ert_gateway_handler_display_create_sensor_text(ert_data_logger_entry *entry, size_t length,
    char *text1, char *text2, char *text3)
{
  ert_sensor_data *data;
  int offset;

  data = ert_gateway_handler_display_get_sensor_data(entry, ERT_SENSOR_TYPE_TEMPERATURE);
  ert_gateway_handler_display_format_sensor_value(data, "T", "%04.1f", length, text1);

  data = ert_gateway_handler_display_get_sensor_data(entry, ERT_SENSOR_TYPE_PRESSURE);
  ert_gateway_handler_display_format_sensor_value(data, "P", "%05.0f", length, text2);

  data = ert_gateway_handler_display_get_sensor_data(entry, ERT_SENSOR_TYPE_HUMIDITY);
  offset = ert_gateway_handler_display_format_sensor_value(data, "H", "%02.0f", length, text3);

  data = ert_gateway_handler_display_get_sensor_data(entry, ERT_SENSOR_TYPE_ACCELEROMETER_MAGNITUDE);
  ert_gateway_handler_display_format_sensor_value(data, "A", "%04.1f", length, text3 + offset);
}

void ert_gateway_handler_cycle_display_mode(ert_gateway_handler_display_context *display_context, int8_t delta)
{
  ert_gateway_handler_display_mode last_mode =
      display_context->gateway->config.gps_config.enabled ? DISPLAY_MODE_LAST_WITH_GPS : DISPLAY_MODE_LAST;

  display_context->display_mode = (display_context->display_mode + delta) % (last_mode + 1);
}

void ert_gateway_handler_display_handle_input(ert_gateway_handler_display_context *display_context)
{
  ert_log_logger *display_logger = display_context->gateway->display_logger;

  if (display_context->dothat_touch != NULL) {
    dothat_touch_read(display_context->dothat_touch);

    if (dothat_touch_is_pressed(display_context->dothat_touch, DOTHAT_TOUCH_BUTTON_UP)) {
      ert_logl_info(display_logger, "Up");
    }
    if (dothat_touch_is_pressed(display_context->dothat_touch, DOTHAT_TOUCH_BUTTON_DOWN)) {
      ert_logl_info(display_logger, "Down");
    }

    if (dothat_touch_is_pressed(display_context->dothat_touch, DOTHAT_TOUCH_BUTTON_LEFT)) {
      ert_gateway_handler_cycle_display_mode(display_context, -1);
    }
    if (dothat_touch_is_pressed(display_context->dothat_touch, DOTHAT_TOUCH_BUTTON_RIGHT)) {
      ert_gateway_handler_cycle_display_mode(display_context, 1);
    }
  }
}

void ert_gateway_handler_display_update_dothat(ert_gateway_handler_display_context *display_context,
    ert_comm_device_status *comm_device_status)
{
  ert_driver_dothat_backlight *backlight = display_context->backlight;
  ert_driver_dothat_led *dothat_led = display_context->dothat_led;
  if (dothat_led != NULL) {
    float signal_strength_percentage = (100.0f * (comm_device_status->current_rssi + - RSSI_MIN) / (float) - RSSI_MIN);
    dothat_led_set_led_graph(dothat_led, signal_strength_percentage);
  }

  if (backlight != NULL) {
    uint32_t rgb_value;

    if (display_context->last_invalid_receive_packet_count != comm_device_status->invalid_received_packet_count) {
      rgb_value = display_context->gateway->config.dothat_config.backlight_error_color;
      display_context->last_invalid_receive_packet_count = comm_device_status->invalid_received_packet_count;
    } else {
      rgb_value = display_context->gateway->config.dothat_config.backlight_color;
    }

    ert_color_rgb rgb = ert_color_value_to_rgb_brightness(rgb_value, display_context->brightness);
    dothat_backlight_set_rgb_all(backlight, rgb.red, rgb.green, rgb.blue);
  }
}

void ert_gateway_handler_display_update_content(ert_gateway_handler_display_context *display_context)
{
  uint32_t text_buffer_size = ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE;
  char text_buffer[text_buffer_size];

  ert_gateway *gateway = display_context->gateway;
  ert_comm_device *comm_device = gateway->comm_device;
  ert_comm_protocol *comm_protocol = gateway->comm_protocol;
  ert_comm_transceiver *comm_transceiver = gateway->comm_transceiver;

  ert_driver_st7036 *display = display_context->display;

  ert_comm_device_status comm_device_status;
  rfm9xw_read_status(comm_device);
  rfm9xw_get_status(comm_device, &comm_device_status);

  ert_gateway_handler_display_update_dothat(display_context, &comm_device_status);

  ert_log_logger *display_logger = display_context->gateway->display_logger;

  st7036_clear(display_context->display);

  switch (display_context->display_mode) {
    case DISPLAY_MODE_COMM_DEVICE:
      ert_gateway_handler_display_create_comm_device_status_text(&comm_device_status, text_buffer_size, text_buffer);
      ert_logl_info(display_logger, "Comm device status: %s", text_buffer);
      st7036_set_cursor_position(display, 0, 0);
      st7036_write(display, text_buffer);

      snprintf(text_buffer, text_buffer_size, "FE: %07.1f", comm_device_status.frequency_error);
      ert_logl_info(display_logger, "Frequency error: %s", text_buffer);

      ert_gateway_handler_display_create_comm_device_counter_text(&comm_device_status, text_buffer_size, text_buffer);
      ert_logl_info(display_logger, "Comm device counters: %s", text_buffer);
      st7036_set_cursor_position(display, 0, 1);
      st7036_write(display, text_buffer);

      ert_gateway_handler_display_create_packet_time_text(&comm_device_status, text_buffer_size, text_buffer);
      ert_logl_info(display_logger, "Comm device packet time: %s", text_buffer);
      st7036_set_cursor_position(display, 0, 2);
      st7036_write(display, text_buffer);
      break;
    case DISPLAY_MODE_NODE_POSITION:
      ert_logl_info(display_logger, "Node position 1: %s", display_context->node_position_text1);
      ert_logl_info(display_logger, "Node position 2: %s", display_context->node_position_text2);
      ert_logl_info(display_logger, "Node position 3: %s", display_context->node_position_text3);

      st7036_set_cursor_position(display, 0, 0);
      st7036_write(display, display_context->node_position_text1);
      st7036_set_cursor_position(display, 0, 1);
      st7036_write(display, display_context->node_position_text2);
      st7036_set_cursor_position(display, 0, 2);
      st7036_write(display, display_context->node_position_text3);
      break;
    case DISPLAY_MODE_NODE_SENSORS:
      ert_logl_info(display_logger, "Node sensors 1: %s", display_context->node_sensors_text1);
      ert_logl_info(display_logger, "Node sensors 2: %s", display_context->node_sensors_text2);
      ert_logl_info(display_logger, "Node sensors 3: %s", display_context->node_sensors_text3);

      st7036_set_cursor_position(display, 0, 0);
      st7036_write(display, display_context->node_sensors_text1);
      st7036_set_cursor_position(display, 0, 1);
      st7036_write(display, display_context->node_sensors_text2);
      st7036_set_cursor_position(display, 0, 2);
      st7036_write(display, display_context->node_sensors_text3);
      break;
    case DISPLAY_MODE_GATEWAY_POSITION:
      ert_logl_info(display_logger, "Gateway position 1: %s", display_context->gateway_position_text1);
      ert_logl_info(display_logger, "Gateway position 2: %s", display_context->gateway_position_text2);
      ert_logl_info(display_logger, "Gateway position 3: %s", display_context->gateway_position_text3);

      st7036_set_cursor_position(display, 0, 0);
      st7036_write(display, display_context->gateway_position_text1);
      st7036_set_cursor_position(display, 0, 1);
      st7036_write(display, display_context->gateway_position_text2);
      st7036_set_cursor_position(display, 0, 2);
      st7036_write(display, display_context->gateway_position_text3);
      break;
  }
}

void ert_gateway_handler_display_telemetry_node_listener(char *event, void *data, void *context)
{
  ert_gateway_handler_display_context *display_context = (ert_gateway_handler_display_context *) context;
  ert_gateway *gateway = display_context->gateway;
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  ert_gateway_handler_display_create_position_text(entry, ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE,
      display_context->node_position_text1, display_context->node_position_text2, display_context->node_position_text3);

  ert_gateway_handler_display_create_sensor_text(entry, ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE,
      display_context->node_sensors_text1, display_context->node_sensors_text2, display_context->node_sensors_text3);
}

void ert_gateway_handler_display_telemetry_gateway_listener(char *event, void *data, void *context)
{
  ert_gateway_handler_display_context *display_context = (ert_gateway_handler_display_context *) context;
  ert_gateway *gateway = display_context->gateway;
  ert_data_logger_entry *entry = (ert_data_logger_entry *) data;

  ert_gateway_handler_display_create_position_text(entry, ERT_GATEWAY_DISPLAY_TEXT_BUFFER_SIZE,
      display_context->gateway_position_text1, display_context->gateway_position_text2, display_context->gateway_position_text3);
}

void ert_gateway_handler_display_handle_display_mode_cycling(ert_gateway_handler_display_context *display_context)
{
  uint32_t cycle_length = display_context->gateway->config.handler_display_config.display_mode_cycle_length_in_update_intervals;

  if (cycle_length == 0) {
    return;
  }

  display_context->display_update_counter = (display_context->display_update_counter + 1) % cycle_length;

  if (display_context->display_update_counter == 0) {
    ert_gateway_handler_cycle_display_mode(display_context, 1);
  }
}

void *ert_gateway_handler_display(void *context)
{
  int result;

  ert_gateway *gateway = (ert_gateway *) context;

  if (!gateway->config.st7036_enabled) {
    ert_log_info("ST7036 display disabled");
    return NULL;
  }

  ert_gateway_handler_display_context display_context = {0};

  result = ert_gateway_handler_display_context_initialize(gateway, &display_context);
  if (result < 0) {
    return NULL;
  }

  ert_event_emitter_add_listener(gateway->event_emitter, ERT_EVENT_NODE_TELEMETRY_RECEIVED,
      ert_gateway_handler_display_telemetry_node_listener, &display_context);
  ert_event_emitter_add_listener(gateway->event_emitter, ERT_EVENT_GATEWAY_TELEMETRY_RECEIVED,
      ert_gateway_handler_display_telemetry_gateway_listener, &display_context);

  ert_log_info("Display handler thread running");

  while (gateway->running) {
    ert_gateway_handler_display_handle_input(&display_context);

    ert_gateway_handler_display_update_content(&display_context);

    ert_process_sleep_with_interrupt(
        gateway->config.handler_display_config.display_update_interval_seconds, &gateway->running);

    ert_gateway_handler_display_handle_display_mode_cycling(&display_context);
  }

  ert_event_emitter_remove_listener(gateway->event_emitter, ERT_EVENT_NODE_TELEMETRY_RECEIVED,
      ert_gateway_handler_display_telemetry_node_listener);
  ert_event_emitter_remove_listener(gateway->event_emitter, ERT_EVENT_GATEWAY_TELEMETRY_RECEIVED,
      ert_gateway_handler_display_telemetry_gateway_listener);

  ert_gateway_handler_display_context_uninitialize(&display_context);

  ert_log_info("Display handler thread stopping");

  return NULL;
}
