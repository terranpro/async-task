ADD_EXECUTABLE(async-test
  async-test.cpp
  )

IF( CMAKE_ASYNC_COROUTINES )
  ADD_DEFINITIONS(-DAS_USE_COROUTINE_TASKS)
  TARGET_LINK_LIBRARIES( async-test
    boost_context
    )
ENDIF()

TARGET_LINK_LIBRARIES( async-test
  ${pkg-glib_LIBRARIES}
  )
