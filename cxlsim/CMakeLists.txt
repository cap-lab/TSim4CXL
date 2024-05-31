cmake_minimum_required(VERSION 3.14)
project(cxlsim)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DRAMULATOR_DEBUG")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

##### External libraries ####
include(FetchContent)
FetchContent_Declare(
  yaml-cpp                             
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG        yaml-cpp-0.7.0                        
  SOURCE_DIR     ${CMAKE_SOURCE_DIR}/ext/yaml-cpp
)

FetchContent_Declare(
  spdlog                             
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.11.0     
  SOURCE_DIR     ${CMAKE_SOURCE_DIR}/ext/spdlog
)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG        v2.9     
    SOURCE_DIR     ${CMAKE_SOURCE_DIR}/ext/argparse
)
FetchContent_MakeAvailable(yaml-cpp spdlog argparse)

include_directories(
				${yaml-cpp_SOURCE_DIR}/include
				${spdlog_SOURCE_DIR}/include
				${argparse_SOURCE_DIR}/include
				)

include_directories(${CMAKE_SOURCE_DIR}/src)

include_directories(.
				${CMAKE_CURRENT_SOURCE_DIR}/../libs/systemc-3.0.0/include
				${CMAKE_CURRENT_SOURCE_DIR}/include/spdlog/include
				${CMAKE_CURRENT_SOURCE_DIR}/include
				${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/src
				)

link_directories(
				${CMAKE_CURRENT_SOURCE_DIR}/../libs/systemc-3.0.0/lib-linux64
				${CMAKE_CURRENT_SOURCE_DIR}/libs/spdlog
				${CMAKE_CURRENT_SOURCE_DIR}/libs/jsoncpp
				${CMAKE_CURRENT_SOURCE_DIR}/libs
				${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2
				)

link_libraries(rt systemc-3.0.0 spdlog jsoncpp pthread ramulator)

add_definitions(-DROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../")
add_definitions(-DDRAM_RAMULATOR)
add_definitions(-DRAMULATOR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/")

add_executable(cxlsim sc_main.cpp
				components/host.cpp
				components/dram.cpp
				components/wrapper.cpp
				components/configurations.cpp
				components/interconnector.cpp
				components/port.cpp
				utilities/statistics.cpp
				utilities/mm.cpp
				utilities/shmem_communicator.cpp
				utilities/packet_buffer.cpp
        )
