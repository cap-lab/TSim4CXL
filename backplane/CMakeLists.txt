cmake_minimum_required(VERSION 3.14)
project(cxlsim)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DRAMULATOR_DEBUG")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/ext/yaml-cpp/include
	${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/ext/spdlog/include
	${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/ext/argparse/include
	${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/src
	)

include_directories(.
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/systemc-3.0.0/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/jsoncpp/include
	${CMAKE_SOURCE_DIR}/src
	)

link_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/systemc-3.0.0/lib-linux64
	${CMAKE_CURRENT_SOURCE_DIR}/../libs/jsoncpp
	${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2
	)

link_libraries(rt systemc-3.0.0 jsoncpp ramulator)

add_definitions(
	-DROOT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../"
	-DDRAM_RAMULATOR
	-DRAMULATOR_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../ramulator2/"
	)

add_executable(
	cxlsim
	sc_main.cpp
	components/host.cpp
	components/sim_wrapper.cpp
	components/mem_wrapper.cpp
	components/interconnector.cpp
	components/usp.cpp
	components/dsp.cpp
	utilities/statistics.cpp
	utilities/mm.cpp
	utilities/shmem_communicator.cpp
	utilities/packet_buffer.cpp
	utilities/configurations.cpp
	)
