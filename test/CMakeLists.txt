SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_DEBUG_CXX_FLAGS} -O2 -rdynamic -gdwarf-2")

function(CreateTest source_file)

  get_filename_component( exec_name ${source_file} NAME_WE )

  message( "Generating Test: ${exec_name}" )

  ADD_EXECUTABLE( ${exec_name}
    ${source_file}
    )

  TARGET_LINK_LIBRARIES( ${exec_name}
    #  ${pkg-glib_LIBRARIES}
    -pthread
    )

endfunction()

CreateTest( async_performance_test.cpp )
CreateTest( post_performance_test.cpp )
CreateTest( thread_work_performance_test.cpp )
CreateTest( chain_test.cpp )
CreateTest( traits_test.cpp )
