cmake_minimum_required(VERSION 3.10)
project(crashdump)
set(CMAKE_C_STANDARD 99)

# Set the compilers
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)

# Get the user's home directory
set(USER_HOME_DIR $ENV{HOME})

# Location of source code base path
set(BASE_PATH "${USER_HOME_DIR}/src")

# Add the directories containing the header files
include_directories("${BASE_PATH}/libpeci")
include_directories("${BASE_PATH}/cJSON/build/include")
include_directories("${BASE_PATH}/safeclib/include")

# Add the directories containing the libraries
link_directories("${BASE_PATH}/libpeci")
link_directories("${BASE_PATH}/cJSON/build/lib")
link_directories("${BASE_PATH}/safeclib/lib")

# Use 2x1S Configuration
add_definitions (-DUSE_TWO_BY_ONE_S_CONFIGURATION)

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

# override for compatibility reason
add_compile_options(-Wno-char-subscripts)

set(SRC_CORE_FILES
        base64Encode.c
        BigCore.c
        cmdprocessor.c
        consolelog.c
        Crashlog.c
        crashdump.c
        flow.c
        inputparser.c
        logger.c
        TorDump.c
        tpmi.c
        utils.c
        utils_common_custom.c
        validator.c
)

add_library(acd SHARED ${SRC_CORE_FILES})
target_link_libraries(acd peci cjson safec-3.6.0)

set(SRC_MAIN_FILES
        main.c
)

add_executable(crashdump ${SRC_MAIN_FILES})
target_link_libraries(crashdump acd)
