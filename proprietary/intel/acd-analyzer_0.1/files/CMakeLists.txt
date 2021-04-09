##################################################################################
#
# INTEL CONFIDENTIAL
#
# Copyright 2021 Intel Corporation.
#
# This software and the related documents are Intel copyrighted materials, and
# your use of them is governed by the express license under which they were
# provided to you ("License"). Unless the License provides otherwise, you may
# not use, modify, copy, publish, distribute, disclose or transmit this
# software or the related documents without Intel's prior written permission.
#
# This software and the related documents are provided as is, with no express
# or implied warranties, other than those that are expressly stated in
# the License.
#
##################################################################################

cmake_minimum_required(VERSION 3.12)
project(bafi VERSION 1.46)

#IF(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#  include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#  conan_basic_setup()
#ENDIF()

set(VERSION "1.46")
set(SKX_CPU "SKX")
set(CLX_CPU "CLX")
set(CPX_CPU "CPX")
set(ICX_CPU "ICX")
set(SPR_CPU "SPR")
set(ICXD_CPU "ICX-D")
string(APPEND REL_CPUS " - ${SKX_CPU} \\n - ${CLX_CPU} \\n - ${CPX_CPU} \\n - ${ICX_CPU} ")
string(APPEND UNREL_CPUS " - ${SPR_CPU} \\n - ${ICXD_CPU}")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -fstack-protector-all")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -fstack-protector-all")
set(INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable(bafi src/main.cpp)
IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(bafi PUBLIC stdc++fs)
ENDIF()
target_include_directories(bafi PRIVATE include)
target_compile_definitions(bafi PUBLIC VERSION="${VERSION}" RELEASED="${REL_CPUS}" UNRELEASED="${UNREL_CPUS}")
target_compile_features(bafi PUBLIC cxx_std_17)

install (TARGETS bafi
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib/static)

install (
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/default_device_map.json
          ${CMAKE_CURRENT_SOURCE_DIR}/default_memory_map.json
          ${CMAKE_CURRENT_SOURCE_DIR}/default_silkscreen_map.json
    PERMISSIONS OWNER_READ
    DESTINATION /var/bafi/
)
