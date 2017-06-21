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

set(ERTLIB_SUPPORT_GPSD ON CACHE BOOL "Build support for GPSD" FORCE)
set(ERTLIB_SUPPORT_RTIMULIB OFF CACHE BOOL "Do not build support for RTIMULib" FORCE)

option(ERTGATEWAY_SUPPORT_GPSD "Build GPSD support in ertgateway" ON)

set(JANSSON_BUILD_DOCS OFF CACHE BOOL "Do not build Jansson docs" FORCE)
set(JANSSON_BUILD_SHARED_LIBS ON CACHE BOOL "Build Jansson shared library" FORCE)
set(JANSSON_WITHOUT_TESTS ON CACHE BOOL "Build Jansson without tests" FORCE)
set(JANSSON_EXAMPLES OFF CACHE BOOL "Do not build Jansson examples" FORCE)

project(ertgateway)

use_c11()

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-funwind-tables)

# add_definitions(-DERTLIB_ENABLE_LOG_DEBUG)

IF (ERTGATEWAY_SUPPORT_GPSD)
  add_definitions(-DERTLIB_SUPPORT_GPSD)
  add_definitions(-DERTGATEWAY_SUPPORT_GPSD)
ENDIF ()

add_subdirectory(../libertapp build/libertapp)

include_directories(../libert
    ../libertapp
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ../deps/jansson/src
    ${JANSSON_INCLUDE_DIRS})

add_executable(ertgateway
    ertgateway.c ertgateway.h ertgateway-handler-display.c ertgateway-handler-display.h
    ertgateway-handler-telemetry-node.c ertgateway-handler-telemetry-node.h
    ertgateway-handler-telemetry-gateway.c ertgateway-handler-telemetry-gateway.h
    ertgateway-handler-image.c ertgateway-handler-image.h
    ertgateway-config.c ertgateway-config.h ertgateway-server.c ertgateway-server.h ertgateway-common.h
    ertgateway-stream-listener.c ertgateway-stream-listener.h)
target_link_libraries(ertgateway ert ertapp websockets_shared jansson)

configure_file(${PROJECT_SOURCE_DIR}/zlog.conf ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertgateway-start.sh ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertgateway-start-dev.sh ${PROJECT_BINARY_DIR} COPYONLY)

IF (NOT EXISTS "${PROJECT_BINARY_DIR}/ertgateway.yaml")
  configure_file(${PROJECT_SOURCE_DIR}/ertgateway.yaml ${PROJECT_BINARY_DIR} COPYONLY)
ENDIF ()

install(TARGETS ertgateway DESTINATION bin)
