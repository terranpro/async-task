ADD_EXECUTABLE(async-test
  async-test.cpp
  )

IF( CMAKE_ASYNC_COROUTINES )
  ADD_DEFINITIONS(-DAS_USE_COROUTINE_TASKS)
  TARGET_LINK_LIBRARIES( async-test
    boost_context
    )
ENDIF()

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_DEBUG_CXX_FLAGS} -O2 -rdynamic -gdwarf-2")

TARGET_LINK_LIBRARIES( async-test
  #  ${pkg-glib_LIBRARIES}
  -pthread
  )
