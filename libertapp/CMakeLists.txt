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

set(LWS_WITH_STATIC OFF CACHE BOOL "Do not build static library for libwebsockets" FORCE)
set(LWS_WITH_SHARED ON CACHE BOOL "Build shared library for libwebsockets" FORCE)
set(LWS_WITH_SSL OFF CACHE BOOL "Do not build SSL support for libwebsockets" FORCE)
set(LWS_WITH_ZLIB ON CACHE BOOL "Build zlib support for libwebsockets" FORCE)
set(LWS_WITH_HTTP2 ON CACHE BOOL "Build HTTP/2 support for libwebsockets" FORCE)

set(LWS_WITHOUT_CLIENT ON CACHE BOOL "Do not build client support for libwebsockets" FORCE)
set(LWS_WITHOUT_SERVER OFF CACHE BOOL "Build server support for libwebsockets" FORCE)

set(LWS_LINK_TESTAPPS_DYNAMIC ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_SERVER_EXTPOLL ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_PING ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_ECHO ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_CLIENT ON CACHE BOOL "" FORCE)
set(LWS_WITHOUT_TEST_FRAGGLE ON CACHE BOOL "" FORCE)

project(libertapp)

use_c11()

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-funwind-tables)

set(libertapp_HEADERS ert-fileutil.h
    ert-data-logger-history-reader.h
    ert-server.h ert-server-session.h ert-server-session-http.h ert-server-session-websocket.h ert-server-config.h
    ert-handler-image-helpers.h ert-server-status.h)

set(libertapp_SOURCES ert-fileutil.c
    ert-data-logger-history-reader.c
    ert-server.c ert-server-session.c ert-server-session-http.c ert-server-session-websocket.c ert-server-config.c
    ert-handler-image-helpers.c ert-server-status.c)

set(libertapp_LIBS rt ert websockets_shared jansson)

add_subdirectory(../libert build/libert)
add_subdirectory(../deps/libwebsockets build/libwebsockets)

include_directories(../libert
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
    ../deps/jansson/src
    ${JANSSON_INCLUDE_DIRS})

add_library(ertapp STATIC ${libertapp_SOURCES})
target_link_libraries(ertapp ${libertapp_LIBS})

install(TARGETS ertapp DESTINATION lib)
install(FILES ${libertapp_HEADERS} DESTINATION include)
