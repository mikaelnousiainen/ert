#ifndef __ERT_EXIF_H
#define __ERT_EXIF_H

#include "ert-common.h"
#include "ert-gps.h"

#define EXIF_TAG_GPS_VERSION_ID "GPSVersionID"

#define EXIF_TAG_GPS_LATITUDE_REF "GPSLatitudeRef"
#define EXIF_TAG_GPS_LATITUDE "GPSLatitude"
#define EXIF_TAG_GPS_LONGITUDE_REF "GPSLongitudeRef"
#define EXIF_TAG_GPS_LONGITUDE "GPSLongitude"
#define EXIF_TAG_GPS_ALTITUDE_REF "GPSAltitudeRef"
#define EXIF_TAG_GPS_ALTITUDE "GPSAltitude"
#define EXIF_TAG_GPS_SATELLITES "GPSSatellites"
#define EXIF_TAG_GPS_STATUS "GPSStatus"
#define EXIF_TAG_GPS_MEASURE_MODE "GPSMeasureMode"
#define EXIF_TAG_GPS_SPEED_REF "GPSSpeedRef"
#define EXIF_TAG_GPS_SPEED "GPSSpeed"
#define EXIF_TAG_GPS_TRACK_REF "GPSTrackRef"
#define EXIF_TAG_GPS_TRACK "GPSTrack"
#define EXIF_TAG_GPS_TIME_STAMP "GPSTimeStamp"
#define EXIF_TAG_GPS_DATE_STAMP "GPSDateStamp"

#define EXIF_GPS_LATITUDE_REF_NORTH "N"
#define EXIF_GPS_LATITUDE_REF_SOUTH "S"

#define EXIF_GPS_LONGITUDE_REF_EAST "E"
#define EXIF_GPS_LONGITUDE_REF_WEST "W"

#define EXIF_GPS_ALTITUDE_REF_ABOVE_SEA_LEVEL "0"
#define EXIF_GPS_ALTITUDE_REF_BELOW_SEA_LEVEL "1"

#define EXIF_GPS_STATUS_MEASUREMENT_ACTIVE "A"
#define EXIF_GPS_STATUS_MEASUREMENT_VOID "V"

#define EXIF_GPS_MEASURE_MODE_2D "2"
#define EXIF_GPS_MEASURE_MODE_3D "3"

#define EXIF_GPS_SPEED_REF_KILOMETERS_PER_HOUR "K"
#define EXIF_GPS_SPEED_REF_MILES_PER_HOUR "M"
#define EXIF_GPS_SPEED_REF_KNOTS "N"

#define EXIF_GPS_TRACK_REF_MAGNETIC_NORTH "M"
#define EXIF_GPS_TRACK_REF_TRUE_NORTH "T"

#define ERT_EXIF_ENTRY_TAG_LENGTH 128
#define ERT_EXIF_ENTRY_VALUE_LENGTH 256

typedef struct _ert_exif_entry {
  char tag[ERT_EXIF_ENTRY_TAG_LENGTH];
  char value[ERT_EXIF_ENTRY_VALUE_LENGTH];
} ert_exif_entry;

typedef struct _ert_exif_dms {
  bool negative;
  uint32_t degrees;
  uint32_t degrees_denominator;
  uint32_t minutes;
  uint32_t minutes_denominator;
  uint32_t seconds;
  uint32_t seconds_denominator;
} ert_exif_dms;

int ert_exif_gps_create(ert_gps_data *gps_data, ert_exif_entry **entries_rcv);

#endif
