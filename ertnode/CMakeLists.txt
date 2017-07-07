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

project(ertnode)

use_c11()

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-funwind-tables)

# add_definitions(-DERTLIB_ENABLE_LOG_DEBUG)

include_directories(../libert
    ../libertapp)
add_subdirectory(../libertapp build/libertapp)

add_executable(ertnode
    ertnode.c ertnode.h ertnode-common.h
    ertnode-config.c ertnode-config.h
    ertnode-image.c ertnode-image.h
    ertnode-server.c ertnode-server.h
    ertnode-collector-telemetry.c ertnode-collector-telemetry.h
    ertnode-collector-image.c ertnode-collector-image.h
    ertnode-sender-telemetry-comm.c ertnode-sender-telemetry-comm.h
    ertnode-sender-telemetry-gsm.c ertnode-sender-telemetry-gsm.h
    ertnode-sender-image-comm.c ertnode-sender-image-comm.h)
target_link_libraries(ertnode ert ertapp)

configure_file(${PROJECT_SOURCE_DIR}/zlog.conf ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertnode-start.sh ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertnode-prepare-raspbian.sh ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertnode-start-dev.sh ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertnode-check.sh ${PROJECT_BINARY_DIR} COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/ertnode-watch.sh ${PROJECT_BINARY_DIR} COPYONLY)

IF (NOT EXISTS "${PROJECT_BINARY_DIR}/ertnode.yaml")
  configure_file(${PROJECT_SOURCE_DIR}/ertnode.yaml ${PROJECT_BINARY_DIR} COPYONLY)
ENDIF ()

install(TARGETS ertnode DESTINATION bin)
