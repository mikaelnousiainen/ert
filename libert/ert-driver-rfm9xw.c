/*
 * Embedded Radio Tracker
 *
 * Copyright (C) 2017 Mikael Nousiainen
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>

#include "ert-log.h"
#include "ert-time.h"
#include "ert-hal-gpio.h"
#include "ert-driver-rfm9xw.h"

#define RFM9XW_MODE_CHANGE_TIMEOUT  5000

#define RFM9XW_CRYSTAL_FREQUENCY    32000000
#define RFM9XW_FREQUENCY_STEP       ((double) RFM9XW_CRYSTAL_FREQUENCY / (double) (1 << 19))

#define REG_FLAG_WRITE              0x80

// Common registers
#define REG_FIFO                    0x00
#define REG_OPMODE                  0x01

#define REG_RF_CARRIER_FREQ_MSB     0x06
#define REG_RF_CARRIER_FREQ_MID     0x07
#define REG_RF_CARRIER_FREQ_LSB     0x08
#define REG_PA_CONFIG               0x09
#define REG_PA_RAMP                 0x0A
#define REG_OVER_CURRENT_PROT       0x0B
#define REG_LNA                     0x0C

#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_VERSION                 0x42
#define REG_TCXO                    0x4B
#define REG_PA_DAC                  0x4D
#define REG_FORMER_TEMPERATURE      0x5B

#define REG_AGC_REF                 0x61
#define REG_AGC_THRESHOLD_1         0x62
#define REG_AGC_THRESHOLD_2         0x63
#define REG_AGC_THRESHOLD_3         0x64
#define REG_PLL_BANDWIDTH           0x70

// LoRa mode registers
#define REG_LORA_FIFO_ADDR_PTR           0x0D
#define REG_LORA_FIFO_TX_BASE_ADDR       0x0E
#define REG_LORA_FIFO_RX_BASE_ADDR       0x0F
#define REG_LORA_FIFO_RX_CURRENT_ADDR    0x10
#define REG_LORA_IRQ_FLAGS_MASK          0x11
#define REG_LORA_IRQ_FLAGS               0x12
#define REG_LORA_RX_NB_BYTES             0x13
#define REG_LORA_RX_HEADER_COUNT_MSB     0x14
#define REG_LORA_RX_HEADER_COUNT_LSB     0x15
#define REG_LORA_RX_PACKET_COUNT_MSB     0x16
#define REG_LORA_RX_PACKET_COUNT_LSB     0x17
#define REG_LORA_MODEM_STATUS            0x18
#define REG_LORA_PACKET_SNR              0x19
#define REG_LORA_PACKET_RSSI             0x1A
#define REG_LORA_CURRENT_RSSI            0x1B
#define REG_LORA_HOP_CHANNEL             0x1C
#define REG_LORA_MODEM_CONFIG_1          0x1D
#define REG_LORA_MODEM_CONFIG_2          0x1E
#define REG_LORA_SYMB_TIMEOUT_LSB        0x1F

#define REG_LORA_PREAMBLE_MSB            0x20
#define REG_LORA_PREAMBLE_LSB            0x21
#define REG_LORA_PAYLOAD_LENGTH          0x22
#define REG_LORA_MAX_PAYLOAD_LENGTH      0x23
#define REG_LORA_HOP_PERIOD              0x24
#define REG_LORA_FIFO_RX_BYTE_ADDR       0x25
#define REG_LORA_MODEM_CONFIG_3          0x26
#define REG_LORA_FREQ_ERROR_MSB          0x28
#define REG_LORA_FREQ_ERROR_MID          0x29
#define REG_LORA_FREQ_ERROR_LSB          0x2A
#define REG_LORA_RSSI_WIDEBAND           0x2C
#define REG_LORA_DETECTION_OPTIMIZE      0x31
#define REG_LORA_INVERT_IQ               0x33
#define REG_LORA_DETECTION_THRESHOLD     0x37
#define REG_LORA_INVERT_IQ_2             0x3B

// FSK/OOK mode registers
#define REG_FSK_TEMPERATURE              0x3C

// Modes
#define MODE_LORA_SLEEP                  0x00
#define MODE_LORA_STANDBY                0x01
#define MODE_LORA_FSTX                   0x02
#define MODE_LORA_TX                     0x03
#define MODE_LORA_FSRX                   0x04
#define MODE_LORA_RX_CONTINUOUS          0x05
#define MODE_LORA_RX_SINGLE              0x06
#define MODE_LORA_CAD                    0x07
#define MODE_LORA_FLAG_ACCESS_SHARED_REG 0x40
#define MODE_FLAG_LORA                   0x80

// Modem Config 2
#define CRC_OFF                     0x00
#define CRC_ON                      0x04

// Modem Config 3
#define MC3_AGC_AUTO_ON             0x04
#define MC3_LOW_DATA_RATE_OPTIMIZE  0x08

// Modem Status
#define STATUS_SIGNAL_DETECTED      0x01
#define STATUS_SIGNAL_SYNCHRONIZED  0x02
#define STATUS_RX_ACTIVE            0x04
#define STATUS_HEADER_INFO_VALID    0x08
#define STATUS_MODEM_CLEAR          0x10

// Detection Optimize
#define DETECTION_OPTIMIZE_SF6      0x05
#define DETECTION_OPTIMIZE_SF7_TO_SF12 0x03

// Detection Threshold
#define DETECTION_THRESHOLD_SF6     0x0C
#define DETECTION_THRESHOLD_SF7_TO_SF12 0x0A

// Low Noise Amplifier
#define LNA_GAIN_OFF                0x00
#define LNA_GAIN_G1                 0x01
#define LNA_GAIN_G2                 0x02
#define LNA_GAIN_G3                 0x03
#define LNA_GAIN_G4                 0x04
#define LNA_GAIN_G5                 0x05
#define LNA_GAIN_G6                 0x06
#define LNA_GAIN_MIN                LNA_GAIN_G6
#define LNA_GAIN_MED                LNA_GAIN_G4
#define LNA_GAIN_MAX                LNA_GAIN_G1

#define LNA_BOOST_LF_OFF            0x00
#define LNA_BOOST_HF_OFF            0x00
#define LNA_BOOST_HF_ON             0x03

// Invert IQ
#define LORA_INVERT_IQ_RX_MASK     0xBF
#define LORA_INVERT_IQ_RX_OFF      0x00
#define LORA_INVERT_IQ_RX_ON       0x40
#define LORA_INVERT_IQ_TX_MASK     0xFE
#define LORA_INVERT_IQ_TX_OFF      0x01
#define LORA_INVERT_IQ_TX_ON       0x00
#define LORA_INVERT_IQ_2_ON        0x19
#define LORA_INVERT_IQ_2_OFF       0x1D

// IRQ flags
#define IRQ_FLAG_CAD_DETECTED       0x01
#define IRQ_FLAG_FHSS_CHANGE_CHANNEL 0x02
#define IRQ_FLAG_CAD_DONE           0x04
#define IRQ_FLAG_TX_DONE            0x08
#define IRQ_FLAG_VALID_HEADER       0x10
#define IRQ_FLAG_PAYLOAD_CRC_ERROR  0x20
#define IRQ_FLAG_RX_DONE            0x40
#define IRQ_FLAG_RX_TIMEOUT         0x80

// IRQ mask
#define IRQ_MASK_CAD_DETECTED       0x01
#define IRQ_MASK_FHSS_CHANGE_CHANNEL 0x02
#define IRQ_MASK_CAD_DONE           0x04
#define IRQ_MASK_TX_DONE            0x08
#define IRQ_MASK_VALID_HEADER       0x10
#define IRQ_MASK_PAYLOAD_CRC_ERROR  0x20
#define IRQ_MASK_RX_DONE            0x40
#define IRQ_MASK_RX_TIMEOUT         0x80

typedef enum _rfm9xw_counter_type {
  RFM9XW_COUNTER_TYPE_TRANSMIT = 1,
  RFM9XW_COUNTER_TYPE_RECEIVE,
  RFM9XW_COUNTER_TYPE_RECEIVE_INVALID,
} rfm9xw_counter_type;

ert_comm_driver ert_comm_driver_rfm9xw;

typedef bool (*cond_func)(ert_comm_device *);

static int rfm9xw_increment_counter(ert_comm_device *device, rfm9xw_counter_type type, uint64_t packet_bytes)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result = 0;

  pthread_mutex_lock(&driver->status_mutex);

  ert_comm_device_status *status = &device->status;

  switch (type) {
    case RFM9XW_COUNTER_TYPE_TRANSMIT:
      ert_get_current_timestamp(&status->last_transmitted_packet_timestamp);
      status->transmitted_packet_count++;
      status->transmitted_bytes += packet_bytes;
      break;
    case RFM9XW_COUNTER_TYPE_RECEIVE:
      ert_get_current_timestamp(&status->last_received_packet_timestamp);
      status->received_packet_count++;
      status->received_bytes += packet_bytes;
      break;
    case RFM9XW_COUNTER_TYPE_RECEIVE_INVALID:
      ert_get_current_timestamp(&status->last_invalid_received_packet_timestamp);
      status->invalid_received_packet_count++;
      break;
    default:
      ert_log_error("Invalid counter type: %d", type);
      result = -EINVAL;
      break;
  }

  pthread_mutex_unlock(&driver->status_mutex);

  return result;
}

static int rfm9xw_cond_timedwait(ert_comm_device *device,
    pthread_cond_t *cond, pthread_mutex_t *mutex, uint32_t milliseconds, cond_func is_condition_met)
{
  int result = 0;
  struct timespec to;

  result = ert_get_current_timestamp_offset(&to, milliseconds);
  if (result < 0) {
    return -EIO;
  }

  pthread_mutex_lock(mutex);
  if (!is_condition_met(device)) {
    result = pthread_cond_timedwait(cond, mutex, &to);
  }
  pthread_mutex_unlock(mutex);

  if (result == ETIMEDOUT) {
    return -ETIMEDOUT;
  } else if (result != 0) {
    ert_log_error("pthread_cond_timedwait failed with result %d", result);
    return -EIO;
  }

  return 0;
}

static int rfm9xw_cond_signal(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
  int result;

  pthread_mutex_lock(mutex);
  result = pthread_cond_signal(cond);
  pthread_mutex_unlock(mutex);

  return result;
}

static int rfm9xw_write_reg(ert_comm_device *device, uint8_t reg, uint8_t value)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t data[2];

  data[0] = reg | REG_FLAG_WRITE;
  data[1] = value;

  int result = hal_spi_transfer(driver->spi_device, 2, data);
  if (result < 0) {
    ert_log_error("Error writing RFM9xW register 0x%02X with value 0x%02X", reg, value);
  }

  return result;
}

static int rfm9xw_read_reg(ert_comm_device *device, uint8_t reg, uint8_t *value)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;
  uint8_t data[2];

  data[0] = reg & 0x7F;
  data[1] = 0;

  result = hal_spi_transfer(driver->spi_device, 2, data);

  *value = data[1];

  if (result < 0) {
    ert_log_error("Error reading RFM9xW register 0x%02X", reg);
  }

  return result;
}

static bool rfm9xw_is_mode_change_signal_active(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  return driver->mode_change_signal;
}

static int rfm9xw_wait_for_mode_change(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  result = rfm9xw_cond_timedwait(device,
      &driver->mode_change_cond, &driver->mode_change_mutex,
      driver->mode_change_timeout_milliseconds, rfm9xw_is_mode_change_signal_active);
  driver->mode_change_signal = false;

  return result;
}

static inline ert_driver_rfm9xw_radio_config *rfm9xw_get_active_config(ert_comm_device *device) {
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  switch (driver->config_type_active) {
    case ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT:
      return &driver->config.transmit_config;
    case ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE:
      return &driver->config.receive_config;
    default:
      return NULL;
  }
}

static inline ert_driver_rfm9xw_radio_config *rfm9xw_get_other_config(ert_comm_device *device) {
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  switch (driver->config_type_active) {
    case ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT:
      return &driver->config.receive_config;
    case ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE:
      return &driver->config.transmit_config;
    default:
      return NULL;
  }
}

static inline ert_comm_device_state rfm9xw_convert_driver_state_to_device_state(
    volatile ert_driver_rfm9xw_state driver_state) {
  switch (driver_state) {
    case RFM9XW_DRIVER_STATE_SLEEP:
      return ERT_COMM_DEVICE_STATE_SLEEP;
    case RFM9XW_DRIVER_STATE_STANDBY:
      return ERT_COMM_DEVICE_STATE_STANDBY;
    case RFM9XW_DRIVER_STATE_TRANSMIT:
      return ERT_COMM_DEVICE_STATE_TRANSMIT;
    case RFM9XW_DRIVER_STATE_DETECTION:
      return ERT_COMM_DEVICE_STATE_DETECTION;
    case RFM9XW_DRIVER_STATE_RECEIVE_CONTINUOUS:
      return ERT_COMM_DEVICE_STATE_RECEIVE_CONTINUOUS;
    case RFM9XW_DRIVER_STATE_RECEIVE_SINGLE:
      return ERT_COMM_DEVICE_STATE_RECEIVE_SINGLE;
    default:
      return ERT_COMM_DEVICE_STATE_STANDBY;
  }
}

static int rfm9xw_update_driver_state(ert_comm_device *device, uint8_t mode)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  volatile ert_driver_rfm9xw_state new_driver_state;

  mode &= ~MODE_FLAG_LORA;

  switch (mode) {
    case MODE_LORA_TX:
    case MODE_LORA_FSTX:
      new_driver_state = RFM9XW_DRIVER_STATE_TRANSMIT;
      break;
    case MODE_LORA_CAD:
      new_driver_state = RFM9XW_DRIVER_STATE_STANDBY;
      break;
    case MODE_LORA_RX_SINGLE:
      new_driver_state = RFM9XW_DRIVER_STATE_RECEIVE_SINGLE;
      break;
    case MODE_LORA_RX_CONTINUOUS:
      new_driver_state = RFM9XW_DRIVER_STATE_RECEIVE_CONTINUOUS;
      break;
    case MODE_LORA_FSRX:
      new_driver_state = RFM9XW_DRIVER_STATE_RECEIVE_CONTINUOUS;
      break;
    case MODE_LORA_STANDBY:
      new_driver_state = RFM9XW_DRIVER_STATE_STANDBY;
      break;
    case MODE_LORA_SLEEP:
      new_driver_state = RFM9XW_DRIVER_STATE_SLEEP;
      break;
    default:
      return -1;
  }

  driver->current_mode = mode;
  driver->driver_state = new_driver_state;

  device->status.device_state = rfm9xw_convert_driver_state_to_device_state(new_driver_state);

  return 0;
}

static int rfm9xw_read_mode_and_update_driver_state(ert_comm_device *device)
{
  uint8_t mode;
  int result;

  result = rfm9xw_read_reg(device, REG_OPMODE, &mode);
  if (result < 0) {
    return result;
  }

  return rfm9xw_update_driver_state(device, mode);
}

static int rfm9xw_set_mode(ert_comm_device *device, uint8_t mode)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  if (mode == driver->current_mode) {
    return 0;
  }

  uint8_t value;

  result = rfm9xw_read_reg(device, REG_OPMODE, &value);
  if (result < 0) {
    return -EIO;
  }
  if ((mode | MODE_FLAG_LORA) == value) {
    driver->current_mode = (uint8_t) (value & ~MODE_FLAG_LORA);
    return 0;
  }

  switch (mode) {
  case MODE_LORA_TX:
  case MODE_LORA_FSTX:
    result = rfm9xw_write_reg(device, REG_LNA, LNA_GAIN_OFF);
    if (result < 0) {
      return -EIO;
    }
    value = (uint8_t) ((driver->config.transmit_config.pa_boost ? PA_CONFIG_BOOST_ON : PA_CONFIG_BOOST_OFF)
        | (driver->config.transmit_config.pa_output_power & 0x0F)
        | ((driver->config.transmit_config.pa_max_power & 0x07) << 4));
    result = rfm9xw_write_reg(device, REG_PA_CONFIG, value);
    if (result < 0) {
      return -EIO;
    }
    break;
  case MODE_LORA_CAD:
  case MODE_LORA_RX_CONTINUOUS:
  case MODE_LORA_RX_SINGLE:
  case MODE_LORA_FSRX:
    result = rfm9xw_write_reg(device, REG_PA_CONFIG,
        PA_CONFIG_BOOST_OFF | PA_CONFIG_MAX_POWER_OFF | PA_CONFIG_OUTPUT_POWER_OFF);
    if (result < 0) {
      return -EIO;
    }
    result = rfm9xw_write_reg(device, REG_LNA, LNA_BOOST_HF_ON | (LNA_GAIN_MAX << 5));
    if (result < 0) {
      return -EIO;
    }
    break;
  case MODE_LORA_STANDBY:
    break;
  case MODE_LORA_SLEEP:
    break;
  default:
    return -EINVAL;
  }

  driver->mode_change_signal = false;

  driver->change_to_mode = mode;

  ert_log_debug("Changing RFM9xW mode to mode 0x%02X", mode | MODE_FLAG_LORA);

  result = rfm9xw_write_reg(device, REG_OPMODE, mode | MODE_FLAG_LORA);
  if (result < 0) {
    return -EIO;
  }

  int result_timeout = 0;

  if (mode != MODE_LORA_SLEEP) {
    result_timeout = rfm9xw_wait_for_mode_change(device);
  }

  if (result_timeout == -ETIMEDOUT) {
    ert_log_error("Timed out waiting for RFM9xW mode change to mode 0x%02X", mode | MODE_FLAG_LORA);
  } else if (result_timeout < 0) {
    ert_log_error("Failed waiting for RFM9xW mode change to mode 0x%02X", mode | MODE_FLAG_LORA);
  }

  // Force update of state
  if (mode == MODE_LORA_SLEEP || result_timeout == -ETIMEDOUT) {
    rfm9xw_update_driver_state(device, mode);
  }

  return result_timeout;
}

static int rfm9xw_update_frequency(ert_comm_device *device)
{
  int result;

  ert_driver_rfm9xw_radio_config *radio_config = rfm9xw_get_active_config(device);

  uint32_t freq_value = (uint32_t)(radio_config->frequency / RFM9XW_FREQUENCY_STEP);

  ert_log_debug("Update frequency: %f Hz with frequency register value 0x%06X",
      radio_config->frequency, freq_value);

  result = rfm9xw_write_reg(device, REG_RF_CARRIER_FREQ_MSB, (uint8_t) ((freq_value >> 16) & 0xFF));
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_RF_CARRIER_FREQ_MID, (uint8_t) ((freq_value >> 8) & 0xFF));
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_RF_CARRIER_FREQ_LSB, (uint8_t) (freq_value & 0xFF));
  if (result < 0) {
    return result;
  }

  device->status.frequency = radio_config->frequency;

  return 0;
}

static uint32_t rfm9xw_get_bandwidth_hz(uint8_t bandwidth_setting)
{
  switch (bandwidth_setting) {
    case BANDWIDTH_7K8:
      return 7800;
    case BANDWIDTH_10K4:
      return 10400;
    case BANDWIDTH_15K6:
      return 15600;
    case BANDWIDTH_20K8:
      return 20800;
    case BANDWIDTH_31K25:
      return 31250;
    case BANDWIDTH_41K7:
      return 41700;
    case BANDWIDTH_62K5:
      return 62500;
    case BANDWIDTH_125K:
      return 125000;
    case BANDWIDTH_250K:
      return 250000;
    case BANDWIDTH_500K:
      return 500000;
    default:
      return 0;
  }
}

int rfm9xw_get_frequency_error(ert_comm_device *device, double *frequency_error_hz)
{
  int32_t frequency_error_reg_value;
  uint8_t value;
  int result;

  result = rfm9xw_read_reg(device, REG_LORA_FREQ_ERROR_MSB, &value);
  if (result < 0) {
    return result;
  }
  frequency_error_reg_value = (value & 0x07) << 16;
  bool negative = (value & 0x08) ? true : false;

  result = rfm9xw_read_reg(device, REG_LORA_FREQ_ERROR_MID, &value);
  if (result < 0) {
    return result;
  }
  frequency_error_reg_value |= value << 8;

  result = rfm9xw_read_reg(device, REG_LORA_FREQ_ERROR_LSB, &value);
  if (result < 0) {
    return result;
  }
  frequency_error_reg_value |= value;

  if (negative) {
    frequency_error_reg_value -= 0x80000;
  }

  ert_driver_rfm9xw_radio_config *radio_config = rfm9xw_get_active_config(device);

  uint32_t bandwidth_hz = rfm9xw_get_bandwidth_hz(radio_config->bandwidth);

  // Negate the frequency error so that it can be *added* to the RF center frequency
  *frequency_error_hz =
      -(((double) frequency_error_reg_value) * (1 << 24) / RFM9XW_CRYSTAL_FREQUENCY)
      * (((double) bandwidth_hz) / 500000.0f);

  return 0;
}

static int rfm9xw_update_config(ert_comm_device *device, bool force)
{
  uint8_t value;
  int result;

  result = rfm9xw_set_mode(device, MODE_LORA_SLEEP);
  if (result < 0) {
    return result;
  }

  ert_driver_rfm9xw_radio_config *active_config = rfm9xw_get_active_config(device);
  ert_driver_rfm9xw_radio_config *other_config = rfm9xw_get_other_config(device);

  bool change_frequency = (active_config->frequency != other_config->frequency);
  if (force || change_frequency) {
    result = rfm9xw_update_frequency(device);
    if (result < 0) {
      return result;
    }
  }

  bool change_modem_config_1 =
      (active_config->implicit_header_mode != other_config->implicit_header_mode)
      || (active_config->error_coding_rate != other_config->error_coding_rate)
      || (active_config->bandwidth != other_config->bandwidth);
  if (force || change_modem_config_1) {
    value = (uint8_t) ((active_config->implicit_header_mode ? IMPLICIT_MODE : EXPLICIT_MODE)
      | (active_config->error_coding_rate << 1)
      | (active_config->bandwidth << 4));
    result = rfm9xw_write_reg(device, REG_LORA_MODEM_CONFIG_1, value);
    if (result < 0) {
      return result;
    }
  }

  bool change_receive_timeout_symbols =
      (active_config->receive_timeout_symbols != other_config->receive_timeout_symbols);
  bool change_modem_config_2 =
      (active_config->spreading_factor != other_config->spreading_factor)
      || (active_config->crc != other_config->crc)
      || change_receive_timeout_symbols;
  if (force || change_modem_config_2) {
    value = (uint8_t) ((active_config->spreading_factor << 4) | (active_config->crc ? CRC_ON : CRC_OFF));
    if (active_config->receive_timeout_symbols > 0) {
      value |= ((active_config->receive_timeout_symbols >> 8) & 0x03);
    }
    result = rfm9xw_write_reg(device, REG_LORA_MODEM_CONFIG_2, value);
    if (result < 0) {
      return result;
    }
  }

  if (force || change_receive_timeout_symbols) {
    if (active_config->receive_timeout_symbols > 0) {
      result = rfm9xw_write_reg(device, REG_LORA_SYMB_TIMEOUT_LSB,
          (uint8_t) (active_config->receive_timeout_symbols & 0xFF));
      if (result < 0) {
        return result;
      }
    }
  }

  bool change_modem_config_3 =
      (active_config->low_data_rate_optimize != other_config->low_data_rate_optimize);
  if (force || change_modem_config_3) {
    value = (uint8_t) ((active_config->low_data_rate_optimize ? MC3_LOW_DATA_RATE_OPTIMIZE : 0) | MC3_AGC_AUTO_ON);
    result = rfm9xw_write_reg(device, REG_LORA_MODEM_CONFIG_3, value);
    if (result < 0) {
      return result;
    }
  }

  bool change_spreading_factor =
      (active_config->spreading_factor != other_config->spreading_factor);
  if (force || change_spreading_factor) {
    result = rfm9xw_read_reg(device, REG_LORA_DETECTION_OPTIMIZE, &value);
    if (result < 0) {
      return result;
    }
    value = (uint8_t) ((value & 0xF8) | ((active_config->spreading_factor == SPREADING_6)
                                         ? DETECTION_OPTIMIZE_SF6 : DETECTION_OPTIMIZE_SF7_TO_SF12));
    result = rfm9xw_write_reg(device, REG_LORA_DETECTION_OPTIMIZE, value);
    if (result < 0) {
      return result;
    }

    value = (uint8_t) ((active_config->spreading_factor == SPREADING_6)
                       ? DETECTION_THRESHOLD_SF6 : DETECTION_THRESHOLD_SF7_TO_SF12);
    result = rfm9xw_write_reg(device, REG_LORA_DETECTION_THRESHOLD, value);
    if (result < 0) {
      return result;
    }
  }

  bool change_preamble_length =
      (active_config->preamble_length != other_config->preamble_length);
  if ((active_config->preamble_length > 0) && (force || change_preamble_length)) {
    result = rfm9xw_write_reg(device, REG_LORA_PREAMBLE_MSB,
        (uint8_t) ((active_config->preamble_length >> 8) & 0xFF));
    if (result < 0) {
      return result;
    }
    result = rfm9xw_write_reg(device, REG_LORA_PREAMBLE_LSB,
        (uint8_t) (active_config->preamble_length & 0xFF));
    if (result < 0) {
      return result;
    }
  }

  bool change_expected_payload_length =
      (active_config->expected_payload_length != other_config->expected_payload_length);
  if ((active_config->expected_payload_length > 0) && (force || change_expected_payload_length)) {
    result = rfm9xw_write_reg(device, REG_LORA_PAYLOAD_LENGTH, active_config->expected_payload_length);
    if (result < 0) {
      return result;
    }
  }

  if (force) {
    result = rfm9xw_write_reg(device, REG_LORA_MAX_PAYLOAD_LENGTH, 0xFF);
    if (result < 0) {
      return result;
    }
  }

  bool change_frequency_hop =
      (active_config->frequency_hop_enabled != other_config->frequency_hop_enabled)
      || (active_config->frequency_hop_period != other_config->frequency_hop_period);
  if (force || change_frequency_hop) {
    if (active_config->frequency_hop_enabled) {
      result = rfm9xw_write_reg(device, REG_LORA_HOP_PERIOD, active_config->frequency_hop_period);
    } else {
      result = rfm9xw_write_reg(device, REG_LORA_HOP_PERIOD, 0x00);
    }
    if (result < 0) {
      return result;
    }
  }

  return 0;
}

static int rfm9xw_update_config_for_receive(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  if (driver->config_type_active == ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE) {
    result = rfm9xw_set_mode(device, MODE_LORA_STANDBY);
    if (result < 0) {
      return result;
    }

    return 0;
  }

  driver->config_type_active = ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE;

  result = rfm9xw_update_config(device, false);
  if (result < 0) {
    return result;
  }

  return rfm9xw_set_mode(device, MODE_LORA_STANDBY);
}

static int rfm9xw_update_config_for_transmit(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  if (driver->config_type_active == ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT) {
    result = rfm9xw_set_mode(device, MODE_LORA_STANDBY);
    if (result < 0) {
      return result;
    }

    return 0;
  }

  driver->config_type_active = ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT;

  result = rfm9xw_update_config(device, false);
  if (result < 0) {
    return result;
  }

  return rfm9xw_set_mode(device, MODE_LORA_STANDBY);
}

int rfm9xw_configure(ert_comm_device *device, ert_driver_rfm9xw_config *config)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  memcpy(&driver->config, config, sizeof(ert_driver_rfm9xw_config));

  return rfm9xw_update_config(device, true);
}

int rfm9xw_configure_generic(ert_comm_device *device, void *config)
{
  // TODO: check validity of config
  return rfm9xw_configure(device, (ert_driver_rfm9xw_config *) config);
}

int rfm9xw_set_frequency(ert_comm_device *device, ert_comm_device_config_type type, double frequency)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  ert_driver_rfm9xw_radio_config *radio_config;
  switch (type) {
    case ERT_COMM_DEVICE_CONFIG_TYPE_TRANSMIT:
      radio_config = &driver->config.transmit_config;
      break;
    case ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE:
      radio_config = &driver->config.receive_config;
      break;
    default:
      return -EINVAL;
  }

  radio_config->frequency = frequency;

  return rfm9xw_update_config(device, true);
}

static void rfm9xw_interrupt_dio0(void *arg)
{
  ert_comm_device *device = (ert_comm_device *) arg;
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  uint8_t irq_flags;
  rfm9xw_read_reg(device, REG_LORA_IRQ_FLAGS, &irq_flags);

  // NOTE: driver_state may be incorrect because of spurious interrupts or other mode changes -> check IRQ flags
  bool irq_tx_done = (irq_flags & IRQ_FLAG_TX_DONE) != 0;
  bool irq_rx_done = (irq_flags & IRQ_FLAG_RX_DONE) != 0;
  bool irq_cad_detected = (irq_flags & IRQ_FLAG_CAD_DETECTED) != 0;

  bool handled = false;

  ert_log_debug("dio0 interrupt: before: driver_state=0x%02X irq_flags=%02X tx_done=%d rx_done=%d cad_detected=%d",
      driver->driver_state, irq_flags, irq_tx_done, irq_rx_done, irq_cad_detected);

  if (irq_tx_done) {
    rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS, IRQ_FLAG_TX_DONE);

    driver->transmit_signal = true;

    if (driver->driver_state != RFM9XW_DRIVER_STATE_TRANSMIT) {
      ert_log_warn("Received TX done interrupt, but driver was not in transmit mode");
    }

    rfm9xw_increment_counter(device, RFM9XW_COUNTER_TYPE_TRANSMIT, 0); // TODO: transmit bytes
    rfm9xw_read_mode_and_update_driver_state(device);

    if (driver->static_config.transmit_callback != NULL) {
      driver->static_config.transmit_callback(driver->static_config.callback_context);
    }
    rfm9xw_cond_signal(&driver->transmit_cond, &driver->transmit_mutex);

    handled = true;
  }

  if (irq_rx_done) {
    // NOTE: RX_DONE IRQ flag *MUST* (and will) be cleared *AFTER* receiving the data

    driver->receive_signal = true;

    if (driver->driver_state != RFM9XW_DRIVER_STATE_RECEIVE_CONTINUOUS
        && driver->driver_state != RFM9XW_DRIVER_STATE_RECEIVE_SINGLE) {
      ert_log_warn("Received RX done interrupt, but driver was not in receive mode");
    }

    rfm9xw_read_mode_and_update_driver_state(device);

    if (driver->static_config.receive_callback != NULL) {
      driver->static_config.receive_callback(driver->static_config.callback_context);
    }
    rfm9xw_cond_signal(&driver->receive_cond, &driver->receive_mutex);

    handled = true;
  }

  if (irq_cad_detected) {
    rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS, IRQ_FLAG_CAD_DONE | IRQ_FLAG_CAD_DETECTED);

    if (driver->driver_state != RFM9XW_DRIVER_STATE_DETECTION) {
      ert_log_warn("Received CAD detection interrupt, but driver was not in state detection mode");
    }

    rfm9xw_read_mode_and_update_driver_state(device);

    driver->detection_signal = true;

    if (driver->static_config.detection_callback != NULL) {
      driver->static_config.detection_callback(driver->static_config.callback_context);
    }
    rfm9xw_cond_signal(&driver->detection_cond, &driver->detection_mutex);

    if (driver->static_config.receive_single_after_detection) {
      rfm9xw_start_receive(device, false);
    }

    handled = true;
  }

  if (!handled) {
    ert_log_error("dio0 interrupt: driver_state=0x%02X irq_flags=%02X: Unhandled interrupt ", driver->driver_state, irq_flags);
    rfm9xw_read_mode_and_update_driver_state(device);
  }

  ert_log_debug("dio0 interrupt: after: driver_state=0x%02X irq_flags=%02X ", driver->driver_state, irq_flags);

  sched_yield();
}

static void rfm9xw_interrupt_dio5(void *arg)
{
  ert_comm_device *device = (ert_comm_device *) arg;
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  driver->mode_change_signal = true;

  uint8_t irq_flags;
  rfm9xw_read_reg(device, REG_LORA_IRQ_FLAGS, &irq_flags);

  ert_log_debug("dio5 interrupt: before: driver_state=0x%02X irq_flags=%02X ", driver->driver_state, irq_flags);
  rfm9xw_read_mode_and_update_driver_state(device);
  ert_log_debug("dio5 interrupt: after: driver_state=0x%02X irq_flags=%02X ", driver->driver_state, irq_flags);

  rfm9xw_cond_signal(&driver->mode_change_cond, &driver->mode_change_mutex);

  sched_yield();
}

static int rfm9xw_initialize(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  driver->config_type_active = ERT_COMM_DEVICE_CONFIG_TYPE_RECEIVE;
  driver->driver_state = RFM9XW_DRIVER_STATE_SLEEP;

  hal_gpio_pin_mode(driver->static_config.pin_dio0, HAL_GPIO_PIN_MODE_INPUT);
  hal_gpio_pin_mode(driver->static_config.pin_dio5, HAL_GPIO_PIN_MODE_INPUT);

  result = rfm9xw_write_reg(device, REG_DIO_MAPPING_1, 0x00);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_DIO_MAPPING_2, 0x00);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_read_reg(device, REG_VERSION, &driver->status.chip_version);
  if (result < 0) {
    return result;
  }
  ert_log_info("RFM9xW chip version: 0x%02X", driver->status.chip_version);

  // Mode change may time out while initializing driver
  driver->mode_change_timeout_milliseconds = 500;

  // Set chip to standby before enabling interrupts
  result = rfm9xw_set_mode(device, MODE_LORA_STANDBY);
  if (result < 0) {
    ert_log_info("Timed out changing RFM9xW mode to standby -- interrupts not configured yet");
  }

  bool update_config_failed = false;
  result = rfm9xw_update_config(device, true);
  if (result < 0) {
    ert_log_info("Failed to update RFM9xW configuration -- interrupts probably not configured yet, retrying ...");
    update_config_failed = true;
  }

  // RFM9xW has to be configured properly to keep DIO5 pin stable, otherwise it fluctuates and tracking it freezes Raspberry Pi
  hal_gpio_pin_isr(driver->static_config.pin_dio0, HAL_GPIO_INT_EDGE_RISING, rfm9xw_interrupt_dio0, device);
  hal_gpio_pin_isr(driver->static_config.pin_dio5, HAL_GPIO_INT_EDGE_RISING, rfm9xw_interrupt_dio5, device);

  driver->mode_change_timeout_milliseconds = RFM9XW_MODE_CHANGE_TIMEOUT;

  if (update_config_failed) {
    result = rfm9xw_update_config(device, true);
    if (result < 0) {
      ert_log_error("Failed to update RFM9xW configuration");
      return result;
    }
  }

  result = rfm9xw_write_reg(device, REG_LORA_FIFO_ADDR_PTR, 0);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_read_reg(device, REG_VERSION, &driver->status.chip_version);
  if (result < 0) {
    return result;
  }

  return 0;
}

static int rfm9xw_write_fifo(ert_comm_device *device, uint8_t length, uint8_t *payload)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t data[257];
  int i;

  data[0] = REG_FIFO | REG_FLAG_WRITE;

  for (i = 0; i < length; i++) {
    data[i + 1] = payload[i];
  }

  return hal_spi_transfer(driver->spi_device, length + 1, data);
}

static int rfm9xw_read_fifo(ert_comm_device *device, uint8_t length, uint8_t *buffer)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t data[257];
  int i, result;

  memset(data, 0, 257);
  data[0] = REG_FIFO;

  result = hal_spi_transfer(driver->spi_device, length + 1, data);
  
  for (i = 0; i < length; i++) {
    buffer[i] = data[i + 1];
  }

  return result;
}

static int rfm9xw_set_invert_iq(ert_comm_device *device, bool transmit)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t value;
  int result;

  result = rfm9xw_read_reg(device, REG_LORA_INVERT_IQ, &value);
  if (result < 0) {
    return result;
  }

  value = (value & LORA_INVERT_IQ_TX_MASK) & LORA_INVERT_IQ_RX_MASK;

  ert_driver_rfm9xw_radio_config *radio_config;

  if (transmit) {
    radio_config = &driver->config.transmit_config;
    value |= (radio_config->iq_inverted
              ? (LORA_INVERT_IQ_TX_ON | LORA_INVERT_IQ_RX_OFF)
              : (LORA_INVERT_IQ_TX_OFF | LORA_INVERT_IQ_RX_OFF));
  } else {
    radio_config = &driver->config.receive_config;
    value |= (radio_config->iq_inverted
              ? (LORA_INVERT_IQ_TX_OFF | LORA_INVERT_IQ_RX_ON)
              : (LORA_INVERT_IQ_TX_OFF | LORA_INVERT_IQ_RX_OFF));
  }

  result = rfm9xw_write_reg(device, REG_LORA_INVERT_IQ, value);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_LORA_INVERT_IQ_2,
      (radio_config->iq_inverted ? LORA_INVERT_IQ_2_ON : LORA_INVERT_IQ_2_OFF));
  if (result < 0) {
    return result;
  }

  return 0;
}

int rfm9xw_transmit(ert_comm_device *device, uint32_t length, uint8_t *payload, uint32_t *bytes_transmitted)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  if (length > RFM9XW_LORA_PACKET_LENGTH_MAX) {
    return -EINVAL;
  }

  driver->transmit_signal = false;

  result = rfm9xw_update_config_for_transmit(device);
  if (result < 0) {
    return result;
  }

  // Map TxDone to DIO0
  result = rfm9xw_write_reg(device, REG_DIO_MAPPING_1, 0x40);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS_MASK,
                   IRQ_MASK_CAD_DETECTED |
                   IRQ_MASK_CAD_DONE |
                   IRQ_MASK_RX_DONE |
                   IRQ_MASK_RX_TIMEOUT |
                   IRQ_MASK_PAYLOAD_CRC_ERROR |
                   IRQ_MASK_VALID_HEADER |
                   IRQ_MASK_FHSS_CHANGE_CHANNEL);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_set_invert_iq(device, true);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_reg(device, REG_LORA_FIFO_TX_BASE_ADDR, 0x00);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_LORA_FIFO_ADDR_PTR, 0x00);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_fifo(device, length, payload);
  if (result < 0) {
    return result;
  }

  uint32_t transmitted = (uint32_t) result - 1;

  result = rfm9xw_write_reg(device, REG_LORA_PAYLOAD_LENGTH, length);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_set_mode(device, MODE_LORA_TX);
  if (result < 0) {
    return result;
  }

  *bytes_transmitted = transmitted;

  return 0;
}

static bool rfm9xw_is_transmit_signal_active(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  return driver->transmit_signal;
}

int rfm9xw_wait_for_transmit(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;
  result = rfm9xw_cond_timedwait(device, &driver->transmit_cond, &driver->transmit_mutex, milliseconds,
      rfm9xw_is_transmit_signal_active);
  driver->transmit_signal = false;
  return result;
}

static bool rfm9xw_is_receive_signal_active(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  return driver->receive_signal;
}

int rfm9xw_wait_for_data(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;
  result = rfm9xw_cond_timedwait(device, &driver->receive_cond, &driver->receive_mutex, milliseconds,
      rfm9xw_is_receive_signal_active);
  driver->receive_signal = false;
  return result;
}

int rfm9xw_start_receive(ert_comm_device *device, bool continuous)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  driver->receive_signal = false;

  result = rfm9xw_update_config_for_receive(device);
  if (result < 0) {
    return result;
  }

  // Map RxDone to DIO0
  result = rfm9xw_write_reg(device, REG_DIO_MAPPING_1, 0x00);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS_MASK,
                   IRQ_MASK_CAD_DETECTED |
                   IRQ_MASK_CAD_DONE |
                   IRQ_MASK_TX_DONE |
                   IRQ_MASK_VALID_HEADER |
                   IRQ_MASK_FHSS_CHANGE_CHANNEL);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_set_invert_iq(device, false);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_reg(device, REG_LORA_FIFO_RX_BASE_ADDR, 0x00);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_write_reg(device, REG_LORA_FIFO_ADDR_PTR, 0x00);
  if (result < 0) {
    return result;
  }

  if (driver->config.receive_config.expected_payload_length > 0) {
    result = rfm9xw_write_reg(device, REG_LORA_PAYLOAD_LENGTH, driver->config.receive_config.expected_payload_length);
    if (result < 0) {
      return result;
    }
  }

  result = rfm9xw_set_mode(device, continuous ? MODE_LORA_RX_CONTINUOUS : MODE_LORA_RX_SINGLE);
  if (result < 0) {
    return result;
  }

  return 0;
}

static bool rfm9xw_is_detection_signal_active(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  return driver->detection_signal;
}

int rfm9xw_wait_for_detection(ert_comm_device *device, uint32_t milliseconds)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;
  result = rfm9xw_cond_timedwait(device, &driver->detection_cond, &driver->detection_mutex, milliseconds,
      rfm9xw_is_detection_signal_active);
  driver->detection_signal = false;
  return result;
}

int rfm9xw_start_detection(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  driver->detection_signal = false;

  result = rfm9xw_update_config_for_receive(device);
  if (result < 0) {
    return result;
  }

  // Map CadDone to DIO0
  result = rfm9xw_write_reg(device, REG_DIO_MAPPING_1, 0x80);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_set_invert_iq(device, false);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS_MASK,
      IRQ_MASK_RX_DONE |
      IRQ_MASK_RX_TIMEOUT |
      IRQ_MASK_TX_DONE |
      IRQ_MASK_VALID_HEADER |
      IRQ_MASK_FHSS_CHANGE_CHANNEL);
  if (result < 0) {
    return result;
  }

  result = rfm9xw_set_mode(device, MODE_LORA_CAD);
  if (result < 0) {
    return result;
  }

  return 0;
}

int rfm9xw_standby(ert_comm_device *device)
{
  return rfm9xw_set_mode(device, MODE_LORA_STANDBY);
}

int rfm9xw_sleep(ert_comm_device *device)
{
  return rfm9xw_set_mode(device, MODE_LORA_SLEEP);
}

static int rfm9xw_read_packet_status(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t raw_rssi;
  int8_t raw_snr;
  int result;

  result = rfm9xw_read_reg(device, REG_LORA_PACKET_RSSI, &raw_rssi);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_read_reg(device, REG_LORA_PACKET_SNR, (uint8_t *) &raw_snr);
  if (result < 0) {
    return result;
  }

  float snr = (float) raw_snr / 4;
  float rssi = RFM9XW_RSSI_MINIMUM_HF + raw_rssi;

  if (raw_snr < 0) {
    rssi += snr;
  }

  driver->status.last_packet_rssi_raw = RFM9XW_RSSI_MINIMUM_HF + raw_rssi;
  driver->status.last_packet_snr_raw = snr;
  device->status.last_received_packet_rssi = rssi;

  return 0;
}

int rfm9xw_receive(ert_comm_device *device, uint32_t buffer_length, uint8_t *buffer, uint32_t *bytes_received)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t irq_flags;
  int result;

  result = rfm9xw_read_reg(device, REG_LORA_IRQ_FLAGS, &irq_flags);
  if (result < 0) {
    return -EIO;
  }

  result = rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS, IRQ_FLAG_RX_DONE);
  if (result < 0) {
    return -EIO;
  }

  if (irq_flags & IRQ_FLAG_PAYLOAD_CRC_ERROR) {
    rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS, 0xFF);
    rfm9xw_increment_counter(device, RFM9XW_COUNTER_TYPE_RECEIVE_INVALID, 0);
    rfm9xw_read_packet_status(device);

    return -EBADMSG;
  }

  uint8_t fifo_addr, read_bytes;

  result = rfm9xw_read_reg(device, REG_LORA_FIFO_RX_CURRENT_ADDR, &fifo_addr);
  if (result < 0) {
    return -EIO;
  }
  result = rfm9xw_read_reg(device, REG_LORA_RX_NB_BYTES, &read_bytes);
  if (result < 0) {
    return -EIO;
  }

  result = rfm9xw_write_reg(device, REG_LORA_FIFO_ADDR_PTR, fifo_addr);
  if (result < 0) {
    return -EIO;
  }

  uint8_t transfer_bytes;

  if (read_bytes > buffer_length) {
    transfer_bytes = (uint8_t) buffer_length;
  } else {
    transfer_bytes = read_bytes;
  }

  result = rfm9xw_read_fifo(device, transfer_bytes, buffer);
  if (result < 0) {
    return -EIO;
  }

  result = rfm9xw_write_reg(device, REG_LORA_IRQ_FLAGS, 0xFF);
  if (result < 0) {
    return -EIO;
  }

  rfm9xw_increment_counter(device, RFM9XW_COUNTER_TYPE_RECEIVE, transfer_bytes);

  rfm9xw_read_packet_status(device);

  *bytes_received = transfer_bytes;

  return 0;
}

int rfm9xw_read_status(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  uint8_t modem_status, raw_rssi;
  int result;

  result = ert_get_current_timestamp(&device->status.timestamp);
  if (result != 0) {
    return -EIO;
  }

  result = rfm9xw_read_reg(device, REG_LORA_MODEM_STATUS, &modem_status);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_read_reg(device, REG_LORA_CURRENT_RSSI, &raw_rssi);
  if (result < 0) {
    return result;
  }
  result = rfm9xw_get_frequency_error(device, &device->status.frequency_error);
  if (result < 0) {
    return result;
  }

  float rssi = -157 + raw_rssi;

  device->status.current_rssi = rssi;

  driver->status.signal_detected = (modem_status & STATUS_SIGNAL_DETECTED) ? true : false;
  driver->status.signal_synchronized = (modem_status & STATUS_SIGNAL_SYNCHRONIZED) ? true : false;
  driver->status.rx_active = (modem_status & STATUS_RX_ACTIVE) ? true : false;
  driver->status.header_info_valid = (modem_status & STATUS_HEADER_INFO_VALID) ? true : false;
  driver->status.modem_clear = (modem_status & STATUS_MODEM_CLEAR) ? true : false;

  return 0;
}

int rfm9xw_get_status(ert_comm_device *device, ert_comm_device_status *status)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  pthread_mutex_lock(&driver->status_mutex);

  memcpy(status, &device->status, sizeof(ert_comm_device_status));

  ert_driver_rfm9xw_radio_config *radio_config = rfm9xw_get_active_config(device);

  status->frequency = radio_config->frequency;

  pthread_mutex_unlock(&driver->status_mutex);

  return 0;
}

uint32_t rfm9xw_get_max_packet_length(ert_comm_device *device) {
  return RFM9XW_LORA_PACKET_LENGTH_MAX;
}

int rfm9xw_set_receive_callback(ert_comm_device *device, ert_comm_driver_callback receive_callback)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  driver->static_config.receive_callback = receive_callback;

  return 0;
}

int rfm9xw_set_transmit_callback(ert_comm_device *device, ert_comm_driver_callback transmit_callback)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  driver->static_config.transmit_callback = transmit_callback;

  return 0;
}

int rfm9xw_set_detection_callback(ert_comm_device *device, ert_comm_driver_callback detection_callback)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  driver->static_config.detection_callback = detection_callback;

  return 0;
}

int rfm9xw_set_callback_context(ert_comm_device *device, void *callback_context)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;

  driver->static_config.callback_context = callback_context;

  return 0;
}

int rfm9xw_open(ert_driver_rfm9xw_static_config *static_config,
		ert_driver_rfm9xw_config *config, ert_comm_device **device_rcv)
{
  int result;
  ert_comm_device *device;
  ert_driver_rfm9xw *driver;

  device = malloc(sizeof(ert_comm_device));
  if (device == NULL) {
    ert_log_fatal("Error allocating memory for comm device struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_first;
  }

  memset(device, 0, sizeof(ert_comm_device));

  driver = malloc(sizeof(ert_driver_rfm9xw));
  if (driver == NULL) {
    ert_log_fatal("Error allocating memory for RFM9xW driver struct: %s", strerror(errno));
    result = -ENOMEM;
    goto error_device_malloc;
  }

  memset(driver, 0, sizeof(ert_driver_rfm9xw));
  memcpy(&driver->static_config, static_config, sizeof(ert_driver_rfm9xw_static_config));
  memcpy(&driver->config, config, sizeof(ert_driver_rfm9xw_config));

  result = hal_spi_open(driver->static_config.spi_bus_index, driver->static_config.spi_device_index,
      driver->static_config.spi_clock_speed, 0, &driver->spi_device);
  if (result != 0) {
    ert_log_error("Error opening SPI device: %s", strerror(errno));
    goto error_driver_malloc;
  }

  result = pthread_mutex_init(&driver->mode_change_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing mode change mutex: %s", strerror(errno));
    result = -EIO;
    goto error_spi_open;
  }
  result = pthread_cond_init(&driver->mode_change_cond, NULL);
  if (result != 0) {
    ert_log_error("Error initializing mode change condition: %s", strerror(errno));
    result = -EIO;
    goto error_mode_change_mutex;
  }

  result = pthread_mutex_init(&driver->receive_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing receive mutex: %s", strerror(errno));
    result = -EIO;
    goto error_mode_change_cond;
  }
  result = pthread_cond_init(&driver->receive_cond, NULL);
  if (result != 0) {
    ert_log_error("Error initializing receive condition: %s", strerror(errno));
    result = -EIO;
    goto error_receive_mutex;
  }

  result = pthread_mutex_init(&driver->transmit_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing transmit mutex: %s", strerror(errno));
    result = -EIO;
    goto error_receive_cond;
  }
  result = pthread_cond_init(&driver->transmit_cond, NULL);
  if (result != 0) {
    ert_log_error("Error initializing transmit condition: %s", strerror(errno));
    result = -EIO;
    goto error_transmit_mutex;
  }

  result = pthread_mutex_init(&driver->detection_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing detection mutex: %s", strerror(errno));
    result = -EIO;
    goto error_transmit_cond;
  }
  result = pthread_cond_init(&driver->detection_cond, NULL);
  if (result != 0) {
    ert_log_error("Error initializing detection condition: %s", strerror(errno));
    result = -EIO;
    goto error_detection_mutex;
  }

  result = pthread_mutex_init(&driver->status_mutex, NULL);
  if (result != 0) {
    ert_log_error("Error initializing status mutex: %s", strerror(errno));
    result = -EIO;
    goto error_detection_cond;
  }

  driver->mode_change_timeout_milliseconds = RFM9XW_MODE_CHANGE_TIMEOUT;

  device->driver = &ert_comm_driver_rfm9xw;
  device->priv = driver;

  device->status.name = "RFM9xW";
  device->status.model = "SX127x/RFM9xW";
  device->status.manufacturer = "Semtech/HopeRF";
  device->status.custom = (void *) &driver->status;

  result = rfm9xw_initialize(device);
  if (result != 0) {
    ert_log_error("Error initializing RFM9xW device: %s", strerror(errno));
    result = -EIO;
    goto error_status_mutex;
  }

  *device_rcv = device;

  return 0;

  error_status_mutex:
  pthread_mutex_destroy(&driver->status_mutex);

  error_detection_cond:
  pthread_cond_destroy(&driver->detection_cond);

  error_detection_mutex:
  pthread_mutex_destroy(&driver->detection_mutex);

  error_transmit_cond:
  pthread_cond_destroy(&driver->transmit_cond);

  error_transmit_mutex:
  pthread_mutex_destroy(&driver->transmit_mutex);

  error_receive_cond:
  pthread_cond_destroy(&driver->receive_cond);

  error_receive_mutex:
  pthread_mutex_destroy(&driver->receive_mutex);

  error_mode_change_cond:
  pthread_cond_destroy(&driver->mode_change_cond);

  error_mode_change_mutex:
  pthread_mutex_destroy(&driver->mode_change_mutex);

  error_spi_open:
  hal_spi_close(driver->spi_device);

  error_driver_malloc:
  free(driver);

  error_device_malloc:
  free(device);

  error_first:

  return result;
}

int rfm9xw_close(ert_comm_device *device)
{
  ert_driver_rfm9xw *driver = (ert_driver_rfm9xw *) device->priv;
  int result;

  rfm9xw_standby(device);
  rfm9xw_sleep(device);

  result = hal_spi_close(driver->spi_device);

  pthread_mutex_destroy(&driver->status_mutex);
  pthread_cond_destroy(&driver->detection_cond);
  pthread_mutex_destroy(&driver->detection_mutex);
  pthread_cond_destroy(&driver->transmit_cond);
  pthread_mutex_destroy(&driver->transmit_mutex);
  pthread_cond_destroy(&driver->receive_cond);
  pthread_mutex_destroy(&driver->receive_mutex);
  pthread_cond_destroy(&driver->mode_change_cond);
  pthread_mutex_destroy(&driver->mode_change_mutex);

  free(driver);
  free(device);

  return result;
}

ert_comm_driver ert_comm_driver_rfm9xw = {
    .transmit = rfm9xw_transmit,
    .wait_for_transmit = rfm9xw_wait_for_transmit,
    .start_detection = rfm9xw_start_detection,
    .wait_for_detection = rfm9xw_wait_for_detection,
    .start_receive = rfm9xw_start_receive,
    .wait_for_data = rfm9xw_wait_for_data,
    .receive = rfm9xw_receive,
    .configure = rfm9xw_configure_generic,
    .set_frequency = rfm9xw_set_frequency,
    .get_frequency_error = rfm9xw_get_frequency_error,
    .standby = rfm9xw_standby,
    .read_status = rfm9xw_read_status,
    .get_status = rfm9xw_get_status,
    .get_max_packet_length = rfm9xw_get_max_packet_length,
    .set_receive_callback = rfm9xw_set_receive_callback,
    .set_transmit_callback = rfm9xw_set_transmit_callback,
    .set_detection_callback = rfm9xw_set_detection_callback,
    .set_callback_context = rfm9xw_set_callback_context,
    .sleep = rfm9xw_sleep,
    .close = rfm9xw_close,
};
