#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "ert-exif.h"
#include "ert-time.h"
#include "ert-log.h"

void ert_exif_convert_to_dms_fractional(double degrees, ert_exif_dms *dms)
{
  dms->degrees = (uint32_t) floor(degrees);
  dms->degrees_denominator = 1;

  double minutes = (degrees - (double) dms->degrees) * 60.0f;

  dms->minutes = (uint32_t) floor(minutes);
  dms->minutes_denominator = 1;

  double seconds = (minutes - (double) dms->minutes) * 60.0f;

  dms->seconds = (uint32_t) floor(seconds);
  dms->seconds_denominator = 1;
}

int ert_exif_format_dms_to_fractional(ert_exif_dms *dms, size_t length, char *string)
{
  return snprintf(string, length, "%d/%d,%d/%d,%d/%d", dms->degrees, dms->degrees_denominator,
    dms->minutes, dms->minutes_denominator, dms->seconds, dms->seconds_denominator);
}

int ert_exif_format_degrees_to_dms_fractional(double degrees, size_t length, char *string)
{
  ert_exif_dms dms;
  ert_exif_convert_to_dms_fractional(degrees, &dms);
  return ert_exif_format_dms_to_fractional(&dms, length, string);
}

int ert_exif_format_double_to_fraction(double value, size_t length, char *string)
{
  uint64_t denominator = 10000;
  return snprintf(string, length, "%" PRId64 "/%" PRIu64, (uint64_t) floor(value * (double) denominator), denominator);
}

void ert_exif_format_time(struct timespec *ts, size_t length, char *string)
{
  ert_format_timestamp(ts, "%H/1,%M/1,%S/1", length, (uint8_t *) string);
}

void ert_exif_format_date(struct timespec *ts, size_t length, char *string)
{
  ert_format_timestamp(ts, "%Y:%m:%d", length, (uint8_t *) string);
}

int ert_exif_gps_create(ert_gps_data *gps_data, ert_exif_entry **entries_rcv)
{
  if (gps_data == NULL) {
    return -EINVAL;
  }

  ert_exif_entry *entries = calloc(20, sizeof(ert_exif_entry));
  if (entries == NULL) {
    ert_log_fatal("Error allocating memory for EXIF entries: %s", strerror(errno));
    return -ENOMEM;
  }

  size_t index = 0;

  if (isfinite(gps_data->latitude_degrees)) {
    char *latitude_ref = (gps_data->latitude_degrees >= 0) ? EXIF_GPS_LATITUDE_REF_NORTH : EXIF_GPS_LATITUDE_REF_SOUTH;
    strncpy(entries[index].tag, EXIF_TAG_GPS_LATITUDE_REF, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, latitude_ref, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;

    strncpy(entries[index].tag, EXIF_TAG_GPS_LATITUDE, ERT_EXIF_ENTRY_TAG_LENGTH);
    ert_exif_format_degrees_to_dms_fractional(fabs(gps_data->latitude_degrees), ERT_EXIF_ENTRY_VALUE_LENGTH,
        entries[index].value);
    index++;
  }

  if (isfinite(gps_data->longitude_degrees)) {
    char *longitude_ref = (gps_data->longitude_degrees >= 0) ? EXIF_GPS_LONGITUDE_REF_EAST : EXIF_GPS_LONGITUDE_REF_WEST;
    strncpy(entries[index].tag, EXIF_TAG_GPS_LONGITUDE_REF, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, longitude_ref, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;

    strncpy(entries[index].tag, EXIF_TAG_GPS_LONGITUDE, ERT_EXIF_ENTRY_TAG_LENGTH);
    ert_exif_format_degrees_to_dms_fractional(fabs(gps_data->longitude_degrees), ERT_EXIF_ENTRY_VALUE_LENGTH,
        entries[index].value);
    index++;
  }

  if (isfinite(gps_data->altitude_meters)) {
    char *altitude_ref = (gps_data->altitude_meters >= 0) ? EXIF_GPS_ALTITUDE_REF_ABOVE_SEA_LEVEL : EXIF_GPS_ALTITUDE_REF_BELOW_SEA_LEVEL;
    strncpy(entries[index].tag, EXIF_TAG_GPS_ALTITUDE_REF, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, altitude_ref, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;

    strncpy(entries[index].tag, EXIF_TAG_GPS_ALTITUDE, ERT_EXIF_ENTRY_TAG_LENGTH);
    ert_exif_format_double_to_fraction(fabs(gps_data->altitude_meters), ERT_EXIF_ENTRY_VALUE_LENGTH, entries[index].value);
    index++;
  }

  strncpy(entries[index].tag, EXIF_TAG_GPS_SATELLITES, ERT_EXIF_ENTRY_TAG_LENGTH);
  snprintf(entries[index].value, ERT_EXIF_ENTRY_VALUE_LENGTH, "%d", gps_data->satellites_used);
  index++;

  if (isfinite(gps_data->speed_meters_per_sec)) {
    strncpy(entries[index].tag, EXIF_TAG_GPS_SPEED_REF, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, EXIF_GPS_SPEED_REF_KILOMETERS_PER_HOUR, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;

    strncpy(entries[index].tag, EXIF_TAG_GPS_SPEED, ERT_EXIF_ENTRY_TAG_LENGTH);
    ert_exif_format_double_to_fraction(gps_data->speed_meters_per_sec * 3.6f, ERT_EXIF_ENTRY_VALUE_LENGTH,
        entries[index].value);
    index++;
  }

  char *gps_status = (gps_data->has_fix) ? EXIF_GPS_STATUS_MEASUREMENT_ACTIVE : EXIF_GPS_STATUS_MEASUREMENT_VOID;
  strncpy(entries[index].tag, EXIF_TAG_GPS_STATUS, ERT_EXIF_ENTRY_TAG_LENGTH);
  strncpy(entries[index].value, gps_status, ERT_EXIF_ENTRY_VALUE_LENGTH);
  index++;

  if (gps_data->has_fix) {
    char *gps_measure_mode = (gps_data->mode == ERT_GPS_MODE_FIX_3D) ? EXIF_GPS_MEASURE_MODE_3D : EXIF_GPS_MEASURE_MODE_2D;
    strncpy(entries[index].tag, EXIF_TAG_GPS_MEASURE_MODE, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, gps_measure_mode, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;
  }

  if (isfinite(gps_data->track_degrees)) {
    strncpy(entries[index].tag, EXIF_TAG_GPS_TRACK_REF, ERT_EXIF_ENTRY_TAG_LENGTH);
    strncpy(entries[index].value, EXIF_GPS_TRACK_REF_TRUE_NORTH, ERT_EXIF_ENTRY_VALUE_LENGTH);
    index++;

    strncpy(entries[index].tag, EXIF_TAG_GPS_TRACK, ERT_EXIF_ENTRY_TAG_LENGTH);
    ert_exif_format_double_to_fraction(gps_data->track_degrees, ERT_EXIF_ENTRY_VALUE_LENGTH, entries[index].value);
    index++;
  }

  struct timespec gps_timespec;
  ert_gps_time_seconds_to_timespec(gps_data->time_seconds, &gps_timespec);

  strncpy(entries[index].tag, EXIF_TAG_GPS_TIME_STAMP, ERT_EXIF_ENTRY_TAG_LENGTH);
  ert_exif_format_time(&gps_timespec, ERT_EXIF_ENTRY_VALUE_LENGTH, entries[index].value);
  index++;

  strncpy(entries[index].tag, EXIF_TAG_GPS_DATE_STAMP, ERT_EXIF_ENTRY_TAG_LENGTH);
  ert_exif_format_date(&gps_timespec, ERT_EXIF_ENTRY_VALUE_LENGTH, entries[index].value);
  index++;

  *entries_rcv = entries;

  return 0;
}
