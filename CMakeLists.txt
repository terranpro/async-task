CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(async-task CXX)

INCLUDE(FindPkgConfig)
pkg_check_modules( pkg-glib REQUIRED
  "glib-2.0"
  )

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/async-task
  )

STRING(REPLACE ";" " " glib_CFLAGS_STR "${pkg-glib_CFLAGS}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ${glib_CFLAGS_STR}")

ADD_SUBDIRECTORY(test)
