CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${CMAKE_SOURCE_DIR}/cmake)
include(LinuxCrossCompile)

PROJECT(mediahpp)
SET(PROJECT_VERSION   0.2.0)

Include(CTest)
find_program( MEMORYCHECK_COMMAND valgrind )
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )


SET(CMAKE_CXX_FLAGS " -static --std=c++14 -DASIO_STANDALONE -DASIO_DISABLE_THREADS")

INCLUDE_DIRECTORIES(
    ./include/
)

# UNIT TESTS OF THE CORE API
file(GLOB TESTS
    "${CMAKE_SOURCE_DIR}/tests/*.cpp"
)

file(GLOB EXAMPLES
    "${CMAKE_SOURCE_DIR}/examples/*.cpp"
)

foreach(file ${EXAMPLES})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)
  ADD_EXECUTABLE(${f} ${file})
endforeach(file)

message("Install prefix:            ${CMAKE_INSTALL_PREFIX}")

INSTALL(TARGETS h264-ts-no-audio
  DESTINATION bin
  )

INSTALL(DIRECTORY ./include
  DESTINATION include/${CMAKE_LIBRARY_PATH}/libmediahpp-${PROJECT_VERSION})

#Handle our pkg-config file

#Finally, the pkg-config file
SET(PKG_CONFIG_LIBDIR
    "\${prefix}/lib/\${deb_host_multiarch}"
)
SET(PKG_CONFIG_INCLUDEDIR
    "\${prefix}/include/\${deb_host_multiarch}/mediahpp-${PROJECT_VERSION}/include"
)
SET(PKG_CONFIG_LIBS
    ""
)
SET(PKG_CONFIG_CFLAGS
    "-I\${includedir} ${CMAKE_CXX_FLAGS}"
)

CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
)

INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc"
       DESTINATION lib/${CMAKE_LIBRARY_PATH}/pkgconfig/)
