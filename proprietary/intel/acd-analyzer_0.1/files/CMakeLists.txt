cmake_minimum_required(VERSION 3.8)
project(acd-analyzer VERSION 1.0)

# IF(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#   include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#   conan_basic_setup()
# ENDIF()

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -fstack-protector-all")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -fstack-protector-all")
set(INTERPROCEDURAL_OPTIMIZATION TRUE)

add_executable(acd-analyzer src/main.cpp)
target_include_directories(acd-analyzer PRIVATE include)
target_compile_features(acd-analyzer PUBLIC cxx_std_17)

install (TARGETS acd-analyzer
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib/static)
