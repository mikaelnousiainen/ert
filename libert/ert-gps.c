#include "ert-gps.h"

void ert_gps_time_seconds_to_timespec(double time_seconds, struct timespec *ts)
{
  ts->tv_sec = (__time_t) time_seconds;
  ts->tv_nsec = (uint32_t) ((time_seconds - (uint64_t) time_seconds) * (double) 1000000000);
}
