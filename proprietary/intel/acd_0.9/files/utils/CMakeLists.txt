cmake_minimum_required(VERSION 3.10)
project(crashdump-utils)
set(CMAKE_C_STANDARD 99)

option(ARM_BUILD "Enable arm-linux-gnueabihf building" OFF)
option(DEBUG "Enable debug mode" OFF)
set(BASE_PATH "$ENV{HOME}/src")


if (ARM_BUILD)
    set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    set(CJSON_INCLUDE_DIR "${BASE_PATH}/cjson_arm/include")
    set(SAFECLIB_INCLUDE_DIR "${BASE_PATH}/safeclib_arm/include")
    set(CJSON_LINK_DIR "${BASE_PATH}/cjson_arm/lib")
    set(SAFECLIB_LINK_DIR "${BASE_PATH}/safeclib_arm/lib")
else()
    set(CJSON_INCLUDE_DIR "${BASE_PATH}/cjson_x86/include")
    set(SAFECLIB_INCLUDE_DIR "${BASE_PATH}/safeclib_x86/include")
    set(CJSON_LINK_DIR "${BASE_PATH}/cjson_x86/lib")
    set(SAFECLIB_LINK_DIR "${BASE_PATH}/safeclib_x86/lib")
endif()

include_directories("${CJSON_INCLUDE_DIR}")
include_directories("${SAFECLIB_INCLUDE_DIR}")
link_directories("${CJSON_LINK_DIR}")
link_directories("${SAFECLIB_LINK_DIR}")

add_compile_options(
        -Werror
        -Wall
        -Wuninitialized
        -Wshift-negative-value
        -Wuninitialized
        -Wtype-limits
        -Wmissing-field-initializers
        -Wimplicit-fallthrough=3
        -Wempty-body
        -Wignored-qualifiers
        -Wclobbered
        -Wcast-function-type
)

add_compile_options(-Wno-char-subscripts)
add_executable(crashdump-split crashdump_split.c)
target_link_libraries(crashdump-split safec-3.6.0 cjson)

