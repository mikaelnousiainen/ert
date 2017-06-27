#include <errno.h>

#include "ert-gps-ublox.h"
#include "ert-time.h"
#include "ert-log.h"

// Register descriptions from: u-blox 8 / u-blox M8 Receiver Description - Manual
// Setting of dynamic platform model 6 ("flight mode") based on UKHAS guide: https://ukhas.org.uk/guides:ublox8
static uint8_t ert_gps_ublox_navigation_engine_settings_dynamic_platform_model_airborne[] = {
    0xB5, 0x62, // Message header
    0x06, 0x24, // Message Class 0x06, ID 0x24: UBX-CFG-NAV5 (Navigation Engine Settings)
    0x24, 0x00, // Message length: 36 bytes
    0xFF, 0xFF, // X2 mask: Parameters Bitmask. Only the masked parameters will be applied.
    0x06, // U1 dynModel: Dynamic platform model: 6: airborne with <1g acceleration
    0x03, // U1 fixMode: Position Fixing Mode: 3: auto 2D/3D
    0x00, 0x00, 0x00, 0x00, // I4 fixedAlt: Fixed altitude (mean sea level) for 2D fix mode. (m * 0.01)
    0x10, 0x27, 0x00, 0x00, // U4 fixedAltVar: Fixed altitude variance for 2D mode. (m^2 * 0.0001)
    0x05, // I1 minElev: Minimum Elevation for a GNSS satellite to be used in NAV (deg)
    0x00, // U1 drLimit: Reserved
    0xFA, 0x00, // U2 pDop: Position DOP Mask to use (* 0.1)
    0xFA, 0x00, // U2 tDop: Time DOP Mask to use (* 0.1)
    0x64, 0x00, // U2 pAcc: Position Accuracy Mask (m)
    0x2C, 0x01, // U2 tAcc: Time Accuracy Mask (m)
    0x00, // U1 staticHoldThresh: Static hold threshold (cm/s)
    0x00, // U1 dgnssTimeout: DGNSS timeout (s)
    0x00, // U1 cnoThreshNumSVs: Number of satellites required to have C/N0 above cnoThresh for a fix to be attempted
    0x00, // U1 cnoThresh: C/N0 threshold for deciding whether to attempt a fix (dBHz)
    0x00, 0x00, // U1[2] reserved1: Reserved
    0x00, 0x00, // U2 staticHoldMaxDist: Static hold distance threshold (before quitting static hold) (m)
    0x00, // U1 utcStandard: UTC standard to be used: 0: Automatic; receiver selects based on GNSS configuration (see GNSS time bases).
    0x00, 0x00, 0x00, 0x00, 0x00, // U1[5] reserved2: Reserved
    0x16, 0xDC // Checksum: CK_A CK_B
};

static int ert_gps_ublox_wait_for_acknowledgement(hal_serial_device *serial_device, uint8_t message_class, uint8_t message_id)
{
  uint8_t expected_data[10] = {
      0xB5, 0x62, // Message header
      0x05, 0x01, // Message Class 0x05, ID 0x01
      0x02, 0x00, // Message length: 2 bytes
      message_class, // Class ID of the Acknowledged Message
      message_id, // Message ID of the Acknowledged Message
      0x00, 0x00 // Checksum: CK_A CK_B (to be calculated)
  };
  size_t data_length = sizeof(expected_data);

  // Calculate the checksums
  for (uint8_t i = 2; i < 8; i++) {
    expected_data[8] = expected_data[8] + expected_data[i];
    expected_data[9] = expected_data[9] + expected_data[8];
  }

  int result;
  uint32_t total_bytes_read = 0;
  uint8_t input_byte;
  uint32_t data_index = 0;

  struct timespec start_time;
  result = ert_get_current_timestamp(&start_time);
  if (result < 0) {
    ert_log_error("Error getting current time, result %d", result);
    return -EIO;
  }

  while (data_index < data_length) {
    uint32_t bytes_read;
    result = hal_serial_read(serial_device, 500, 1, &input_byte, &bytes_read);
    if (result < 0) {
      ert_log_error("Error reading UBlox GPS ack from serial port, result %d", result);
      return result;
    }

    total_bytes_read += bytes_read;

    if (input_byte == expected_data[data_index]) {
      data_index++;
    } else {
      data_index = 0;
    }

    if (ert_timespec_diff_milliseconds_from_current(&start_time) > 5000) {
      ert_log_error("Timed out waiting for UBlox GPS ack from serial port");
      return -ETIMEDOUT;
    }
  }

  return 0;
}

int ert_gps_ublox_configure_navigation_engine(hal_serial_device *serial_device)
{
  uint8_t *data = ert_gps_ublox_navigation_engine_settings_dynamic_platform_model_airborne;
  uint32_t length = sizeof(ert_gps_ublox_navigation_engine_settings_dynamic_platform_model_airborne);

  uint32_t bytes_written;
  int result = hal_serial_write(serial_device, length, data, &bytes_written);
  if (result < 0) {
    ert_log_error("Error writing UBlox GPS configuration to serial port, result %d", result);
    return -EIO;
  }

  return ert_gps_ublox_wait_for_acknowledgement(serial_device, data[2], data[3]);
}

int ert_gps_ublox_configure(hal_serial_device_config *serial_device_config)
{
  hal_serial_device *serial_device;

  int result = hal_serial_open(serial_device_config, &serial_device);
  if (result < 0) {
    ert_log_error("Error opening serial port device '%s', result %d", serial_device_config->device, result);
    return result;
  }

  result = ert_gps_ublox_configure_navigation_engine(serial_device);
  if (result < 0) {
    ert_log_error("Error configuring UBlox GPS, result %d", result);
  }

  int close_result = hal_serial_close(serial_device);

  if (result < 0) {
    return result;
  }

  return close_result;
}
