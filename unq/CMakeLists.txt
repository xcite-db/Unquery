cmake_minimum_required(VERSION 3.6.2)
project(unq)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY lib)

include_directories(include
  ../libs/rapidjson
  ../libs
  )

set(SOURCES 
  src/TemplateQuery.cpp
  src/TemplateParser.cpp
  src/params.cpp
  src/utils.cpp
  src/json-utils.cpp
  )

add_executable(unq src/unqlite_main.cpp ${SOURCES})

# target_link_libraries(XCiteDB libre2.a)
