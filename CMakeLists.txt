CMAKE_MINIMUM_REQUIRED(VERSION 2.8.8)

PROJECT(mediahpp)
SET(PROJECT_VERSION   0.2.0)

Include(CTest)
find_program( MEMORYCHECK_COMMAND valgrind )
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )


SET(CMAKE_CXX_FLAGS "--std=c++14 -DASIO_STANDALONE -DASIO_DISABLE_THREADS")

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

#foreach(file ${TESTS})
#  GET_FILENAME_COMPONENT(f ${file} NAME_WE)  
#  ADD_EXECUTABLE(${f} ${file})
#  ADD_TEST(${f} ${f})
#endforeach(file)

foreach(file ${EXAMPLES})
  GET_FILENAME_COMPONENT(f ${file} NAME_WE)  
  ADD_EXECUTABLE(${f} ${file})
endforeach(file)

message("Install prefix:            ${CMAKE_INSTALL_PREFIX}")


