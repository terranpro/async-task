CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(async-task CXX)

INCLUDE(FindPkgConfig)
pkg_check_modules( pkg-glib REQUIRED
  "glib-2.0"
  )

ADD_EXECUTABLE(async-test
  async-test.cpp
  )

STRING(REPLACE ";" " " glib_CFLAGS_STR "${pkg-glib_CFLAGS}")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ${glib_CFLAGS_STR} -pg")

IF( CMAKE_ASYNC_COROUTINES )
  ADD_DEFINITIONS(-DAS_USE_COROUTINE_TASKS)
  TARGET_LINK_LIBRARIES( async-test
    boost_context
    )
ENDIF()

TARGET_LINK_LIBRARIES( async-test
  ${pkg-glib_LIBRARIES}
  )