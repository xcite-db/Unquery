cmake_minimum_required(VERSION 3.6.2)

project(JSONCompare)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/Release)

include_directories(include
  ../libs
  ../libs/shared
  ../libs/rapidjson
  )

set(SOURCES 
  TraverseJSON.cpp
  )

add_executable(JSONCompare JSONCompare.cpp ${SOURCES})

add_executable(JSONGrep JSONgrep.cpp ${SOURCES})

target_link_libraries(JSONGrep stdc++fs)
target_link_libraries(JSONCompare stdc++fs)


