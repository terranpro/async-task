SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_DEBUG_CXX_FLAGS} -O2 -rdynamic -gdwarf-2 -ftemplate-backtrace-limit=0 -std=c++14")

function(CreateTest source_file)

  get_filename_component( exec_name ${source_file} NAME_WE )

  message( "Generating Test: ${exec_name}" )

  ADD_EXECUTABLE( ${exec_name}
    ${source_file}
    )

  TARGET_LINK_LIBRARIES( ${exec_name}
    # ${pkg-glib_LIBRARIES}
    boost_system
    boost_thread
    boost_context
    -pthread
    )

endfunction()

CreateTest( async_performance_test.cpp )
CreateTest( post_performance_test.cpp )
CreateTest( thread_work_performance_test.cpp )
CreateTest( chain_test.cpp )
CreateTest( traits_test.cpp )
CreateTest( await_test.cpp )
CreateTest( sync_test.cpp )
CreateTest( bank_acct_test.cpp )
