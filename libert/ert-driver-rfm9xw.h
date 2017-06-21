/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __ERT_DRIVER_RFM9XW_H
#define __ERT_DRIVER_RFM9XW_H

#include "ert-common.h"
#include "ert-comm.h"
#include "ert-hal-spi.h"
#include <pthread.h>

#define RFM9XW_LORA_PACKET_LENGTH_MAX           0xFF

#define RFM9XW_RSSI_MINIMUM_HF -157
#define RFM9XW_RSSI_MINIMUM_LF -164

// Power Amplifier Config
#define PA_CONFIG_BOOST_OFF         0x00
#define PA_CONFIG_BOOST_ON          0x80
#define PA_CONFIG_OUTPUT_POWER_OFF  0x00
#define PA_CONFIG_OUTPUT_POWER_MIN  0x01
#define PA_CONFIG_OUTPUT_POWER_MED  0x0A
#define PA_CONFIG_OUTPUT_POWER_MAX  0x0F
#define PA_CONFIG_MAX_POWER_OFF     0x00
#define PA_CONFIG_MAX_POWER_MIN     0x01
#define PA_CONFIG_MAX_POWER_MED     0x04
#define PA_CONFIG_MAX_POWER_MAX     0x07

// Modem Config 1
#define EXPLICIT_MODE               0x00
#define IMPLICIT_MODE               0x01

// bits 1..3
#define ERROR_CODING_4_5            0x1
#define ERROR_CODING_4_6            0x2
#define ERROR_CODING_4_7            0x3
#define ERROR_CODING_4_8            0x4

// bits 4..7
#define BANDWIDTH_7K8               0x0
#define BANDWIDTH_10K4              0x1
#define BANDWIDTH_15K6              0x2
#define BANDWIDTH_20K8              0x3
#define BANDWIDTH_31K25             0x4
#define BANDWIDTH_41K7              0x5
#define BANDWIDTH_62K5              0x6
#define BANDWIDTH_125K              0x7
#define BANDWIDTH_250K              0x8
#define BANDWIDTH_500K              0x9

// Modem Config 2
// bits 4..7
#define SPREADING_6                 0x6
#define SPREADING_7                 0x7
#define SPREADING_8                 0x8
#define SPREADING_9                 0x9
#define SPREADING_10                0xA
#define SPREADING_11                0xB
#define SPREADING_12                0xC

typedef volatile enum _ert_driver_rfm9xw_state {
  RFM9XW_DRIVER_STATE_SLEEP = 0x01,
  RFM9XW_DRIVER_STATE_STANDBY = 0x02,
  RFM9XW_DRIVER_STATE_TRANSMIT = 0x11,
  RFM9XW_DRIVER_STATE_DETECTION = 0x21,
  RFM9XW_DRIVER_STATE_RECEIVE_CONTINUOUS = 0x31,
  RFM9XW_DRIVER_STATE_RECEIVE_SINGLE = 0x32
} ert_driver_rfm9xw_state;

typedef struct _ert_driver_rfm9xw_static_config {
	uint16_t spi_bus_index;
	uint16_t spi_device_index;
	uint32_t spi_clock_speed;

	uint8_t pin_dio0;
  uint8_t pin_dio5;

	ert_comm_driver_callback receive_callback;
	ert_comm_driver_callback transmit_callback;
	ert_comm_driver_callback detection_callback;
  void *callback_context;

  bool receive_single_after_detection;
} ert_driver_rfm9xw_static_config;

typedef struct _ert_driver_rfm9xw_radio_config {
  bool pa_boost;
  uint8_t pa_max_power;
  uint8_t pa_output_power;

  double frequency;
  bool frequency_hop_enabled;
  uint8_t frequency_hop_period;
  bool implicit_header_mode;
  uint8_t error_coding_rate;
  uint8_t bandwidth;
  uint8_t spreading_factor;
  bool crc;
  bool low_data_rate_optimize;
  uint16_t preamble_length;
  bool iq_inverted;
  uint16_t receive_timeout_symbols;

  uint8_t expected_payload_length;
} ert_driver_rfm9xw_radio_config;

typedef struct _ert_driver_rfm9xw_config {
  ert_driver_rfm9xw_radio_config transmit_config;
  ert_driver_rfm9xw_radio_config receive_config;
} ert_driver_rfm9xw_config;

typedef struct _ert_driver_rfm9xw_status {
  uint8_t chip_version;

  float last_packet_rssi_raw;
  float last_packet_snr_raw;

  bool modem_clear;
  bool header_info_valid;
  bool rx_active;
  bool signal_synchronized;
  bool signal_detected;
} ert_driver_rfm9xw_status;

typedef struct _ert_driver_rfm9x {
  hal_spi_device *spi_device;

  volatile uint8_t current_mode;
	volatile uint8_t change_to_mode;
	volatile ert_driver_rfm9xw_state driver_state;

	uint32_t mode_change_timeout_milliseconds;

	pthread_mutex_t mode_change_mutex;
	pthread_cond_t mode_change_cond;
  volatile bool mode_change_signal;
	pthread_mutex_t receive_mutex;
	pthread_cond_t receive_cond;
  volatile bool receive_signal;
	pthread_mutex_t transmit_mutex;
	pthread_cond_t transmit_cond;
  volatile bool transmit_signal;
  pthread_mutex_t detection_mutex;
  pthread_cond_t detection_cond;
  volatile bool detection_signal;

  pthread_mutex_t status_mutex;

  ert_driver_rfm9xw_static_config static_config; 
  ert_driver_rfm9xw_config config;
	ert_comm_device_config_type config_type_active;

  ert_driver_rfm9xw_status status;
} ert_driver_rfm9xw;

int rfm9xw_open(ert_driver_rfm9xw_static_config *static_config,
		ert_driver_rfm9xw_config *config, ert_comm_device **device_rcv);
int rfm9xw_close(ert_comm_device *device);

uint32_t rfm9xw_get_max_packet_length(ert_comm_device *device);

int rfm9xw_set_receive_callback(ert_comm_device *device, ert_comm_driver_callback receive_callback);
int rfm9xw_set_transmit_callback(ert_comm_device *device, ert_comm_driver_callback transmit_callback);
int rfm9xw_set_detection_callback(ert_comm_device *device, ert_comm_driver_callback detection_callback);
int rfm9xw_set_callback_context(ert_comm_device *device, void *callback_context);

int rfm9xw_configure(ert_comm_device *device, ert_driver_rfm9xw_config *config);
int rfm9xw_set_frequency(ert_comm_device *device, ert_comm_device_config_type type, double frequency);
int rfm9xw_get_frequency_error(ert_comm_device *device, double *frequency_error_hz);

int rfm9xw_transmit(ert_comm_device *device, uint32_t length, uint8_t *payload, uint32_t *bytes_transmitted);
int rfm9xw_wait_for_transmit(ert_comm_device *device, uint32_t milliseconds);

int rfm9xw_start_detection(ert_comm_device *device);
int rfm9xw_wait_for_detection(ert_comm_device *device, uint32_t milliseconds);

int rfm9xw_start_receive(ert_comm_device *device, bool continuous);
int rfm9xw_wait_for_data(ert_comm_device *device, uint32_t milliseconds);
int rfm9xw_receive(ert_comm_device *device, uint32_t buffer_length, uint8_t *buffer, uint32_t *bytes_received);

int rfm9xw_standby(ert_comm_device *device);
int rfm9xw_sleep(ert_comm_device *device);

int rfm9xw_read_status(ert_comm_device *device);
int rfm9xw_get_status(ert_comm_device *device, ert_comm_device_status *status);

extern ert_comm_driver ert_comm_driver_rfm9xw;

#endif
