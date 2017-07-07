cmake_minimum_required(VERSION 2.8)

macro(use_c11)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=gnu11 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 11)
  endif ()
endmacro(use_c11)

set(ZLOG_BUILD_SHARED_LIBS ON CACHE BOOL "Build zlog shared library" FORCE)
set(ZLOG_BUILD_STATIC_LIBS OFF CACHE BOOL "Do not build zlog static library" FORCE)
set(ZLOG_BUILD_CHK_CONF OFF CACHE BOOL "Do not build zlog-chk-conf" FORCE)

set(JANSSON_BUILD_DOCS OFF CACHE BOOL "Do not build Jansson docs" FORCE)
set(JANSSON_BUILD_SHARED_LIBS ON CACHE BOOL "Build Jansson shared library" FORCE)
set(JANSSON_WITHOUT_TESTS ON CACHE BOOL "Build Jansson without tests" FORCE)
set(JANSSON_EXAMPLES OFF CACHE BOOL "Do not build Jansson examples" FORCE)

set(MSGPACK_ENABLE_SHARED ON CACHE BOOL "Build MsgPack shared library" FORCE)
set(MSGPACK_ENABLE_STATIC OFF CACHE BOOL "Do not build MsgPack static library" FORCE)
set(MSGPACK_BUILD_TESTS OFF CACHE BOOL "Do not build MsgPack tests" FORCE)
set(MSGPACK_BUILD_EXAMPLES OFF CACHE BOOL "Do not build MsgPack examples" FORCE)

set(WIRINGPI_BUILD_DEVLIB OFF CACHE BOOL "Do not build Build WiringPi devLib" FORCE)
set(WIRINGPI_BUILD_GPIO OFF CACHE BOOL "Do not build WiringPi GPIO tool" FORCE)

project(libert)

use_c11()

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-funwind-tables)

OPTION (ERTLIB_SUPPORT_GPSD "Build GPSD support" ON)
OPTION (ERTLIB_SUPPORT_RTIMULIB "Build RTIMULib support" ON)

set(libert_HEADERS ert.h ert-common.h ert-time.h ert-mapper.h ert-mapper-json.h ert-yaml.h ert-event-emitter.h
    ert-hal.h ert-hal-spi.h ert-hal-spi-linux.h ert-hal-common.h
    ert-hal-i2c.h ert-hal-i2c-linux.h
    ert-hal-gpio.h ert-hal-gpio-rpi.h ert-driver-rfm9xw.h ert-driver-rfm9xw-config.h
    ert-driver-st7036.h ert-driver-st7036-config.h
    ert-gps.h ert-gps-ublox.h ert-sensor.h ert-sensor-module-sysinfo.h
    ert-comm.h ert-comm-transceiver.h ert-comm-protocol.h ert-comm-protocol-device-adapter.h
    ert-comm-device-dummy.h ert-comm-protocol-helpers.h ert-comm-protocol-config.h ert-comm-transceiver-config.h
    ert-log.h ert-data-logger.h ert-data-logger-serializer-jansson.h ert-data-logger-writer-zlog.h ert-data-logger-utils.h
    ert-data-logger-serializer-msgpack.h pipe.h ert-pipe.h ert-buffer-pool.h ert-ring-buffer.h
    ert-driver-sn3218.h ert-driver-dothat-backlight.h
    ert-driver-cap1xxx.h ert-driver-dothat-touch.h ert-driver-dothat-led.h
    ert-hal-serial.h ert-hal-serial-posix.h
    ert-driver-gsm-modem.h ert-driver-gsm-modem-config.h ert-color.h ert-exif.h
    geodesic.c ert-geo.h
    ert-image-metadata.h ert-jansson-helpers.h ert-msgpack-helpers.h ert-comm-protocol-json.h
    ert-flight-manager.h)

set(libert_SOURCES ert.c ert-time.c ert-mapper.c ert-mapper-json.c ert-yaml.c ert-event-emitter.c
    ert-hal.c ert-hal-spi.c ert-hal-spi-linux.c
    ert-hal-i2c.c ert-hal-i2c-linux.c
    ert-hal-gpio.c ert-hal-gpio-rpi.c ert-driver-rfm9xw.c ert-driver-rfm9xw-config.c
    ert-driver-st7036.c ert-driver-st7036-config.c
    ert-gps.c ert-gps-ublox.c ert-sensor.c ert-sensor-module-sysinfo.c
    ert-comm.c ert-comm-transceiver.c ert-comm-transceiver.c ert-comm-protocol.c ert-comm-protocol-device-adapter.c
    ert-comm-device-dummy.c ert-comm-protocol-helpers.c ert-comm-protocol-config.c ert-comm-transceiver-config.c
    ert-log.c ert-data-logger.c ert-data-logger-serializer-jansson.c ert-data-logger-writer-zlog.c ert-data-logger-utils.c
    ert-data-logger-serializer-msgpack.c pipe.c ert-pipe.c ert-buffer-pool.c ert-ring-buffer.c ert-process.c ert-process.h
    ert-driver-sn3218.c ert-driver-dothat-backlight.c
    ert-driver-cap1xxx.c ert-driver-dothat-touch.c ert-driver-dothat-led.c
    ert-hal-serial.c ert-hal-serial-posix.c
    ert-driver-gsm-modem.c ert-driver-gsm-modem-config.c ert-color.c ert-exif.c
    geodesic.c ert-geo.c
    ert-image-metadata.c ert-jansson-helpers.c ert-msgpack-helpers.c ert-comm-protocol-json.c
    ert-flight-manager.c)

set(libert_LIBS rt pthread yaml zlog jansson msgpackc libwiringPi)

add_subdirectory(../deps/WiringPi build/wiringPi)
add_subdirectory(../deps/zlog build/zlog)
add_subdirectory(../deps/jansson build/jansson)
add_subdirectory(../deps/msgpack-c build/msgpack-c)

include_directories(
    ../deps/WiringPi/wiringPi
    ../deps/zlog/src
    ../deps/jansson/src
    ${JANSSON_INCLUDE_DIRS}
    ../deps/msgpack-c/include)

IF (ERTLIB_SUPPORT_GPSD)
  add_definitions(-DERTLIB_SUPPORT_GPSD)
  list(APPEND libert_HEADERS ert-gps-driver.h ert-gps-gpsd.h ert-gps-listener.h)
  list(APPEND libert_SOURCES ert-gps-driver.c ert-gps-gpsd.c ert-gps-listener.c)
  list(APPEND libert_LIBS gps)
ENDIF ()

IF (ERTLIB_SUPPORT_RTIMULIB)
  add_definitions(-DERTLIB_SUPPORT_RTIMULIB)
  add_subdirectory(../deps/RTIMULib/RTIMULib build/RTIMULib)
  include_directories(../deps/RTIMULib/RTIMULib)
  list(APPEND libert_HEADERS ert-sensor-module-rtimulib.h)
  list(APPEND libert_SOURCES ert-sensor-module-rtimulib.cpp)
  list(APPEND libert_LIBS RTIMULib)
ENDIF ()

add_library(ert STATIC ${libert_SOURCES})
target_link_libraries(ert ${libert_LIBS})

add_executable(ert_comm_transceiver_test ert-test.c ert-comm-transceiver-test-routines.c ert-comm-transceiver-test.c)
target_link_libraries(ert_comm_transceiver_test ert)

add_executable(ert_comm_protocol_test ert-test.c ert-comm-transceiver-test-routines.c ert-comm-protocol-test.c)
target_link_libraries(ert_comm_protocol_test ert)

enable_testing()

add_test(NAME ert_comm_transceiver_test COMMAND ert_comm_transceiver_test)
add_test(NAME ert_comm_protocol_test COMMAND ert_comm_protocol_test)

install(TARGETS ert DESTINATION lib)
install(FILES ${libert_HEADERS} DESTINATION include)
