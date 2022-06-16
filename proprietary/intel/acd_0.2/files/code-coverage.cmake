# USAGE: To enable any code coverage instrumentation/targets, the single CMake
# option of `CODE_COVERAGE` needs to be set to 'ON'
#

# To add coverage targets, such as calling `make ccov` to generate the actual
# coverage information for perusal or consumption, call
# `crashdump_code_coverage(<TARGET_NAME>)` on an *executable* target.
# ~~~
#
# Example 1: Via target commands
#
# ~~~
# add_library(theLib lib.cpp)
# crashdump_code_coverage(theLib) # As a library target, adds coverage instrumentation but no targets.
#
# add_executable(crashdump_ut main.cpp)
# target_link_libraries(crashdump_ut PRIVATE theLib)
# crashdump_code_coverage(crashdump_ut) # As an executable target, adds the 'ccov-crashdump_ut' target and instrumentation for generating code coverage reports.
# ~~~
#
# Example 2: Target instrumented, but with regex pattern of files to be excluded
# from report
#
# ~~~
# add_executable(crashdump_ut main.cpp non_covered.cpp)
# crashdump_code_coverage(crashdump_ut EXCLUDE non_covered.cpp test/*) # As an executable target, the reports will exclude the non-covered.cpp file, and any files in a test/ folder.
# ~~~

# Options
option(
  CODE_COVERAGE
  "Builds targets with code coverage instrumentation. Requires GCC"
  OFF)

# Programs
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)

# Variables
set(CODE_COVERAGE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/ccov)

# Common initialization/checks
if(CODE_COVERAGE AND NOT CODE_COVERAGE_ADDED)
  set(CODE_COVERAGE_ADDED ON)

  # Common Targets
  add_custom_target(
    ccov-preprocessing
    COMMAND ${CMAKE_COMMAND} -E make_directory
            ${CODE_COVERAGE_OUTPUT_DIR}
    DEPENDS ccov-clean)

  if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                              "GNU")
    # Messages
    message(STATUS "Building with lcov Code Coverage Tools")

    if(CMAKE_BUILD_TYPE)
      string(TOUPPER ${CMAKE_BUILD_TYPE} upper_build_type)
      if(NOT ${upper_build_type} STREQUAL "DEBUG")
        message(
          WARNING
            "Code coverage results with an optimized (non-Debug) build may be misleading"
        )
      endif()
    else()
      message(
        WARNING
          "Code coverage results with an optimized (non-Debug) build may be misleading"
      )
    endif()
    if(NOT LCOV_PATH)
      message(FATAL_ERROR "lcov not found! Aborting...")
    endif()
    if(NOT GENHTML_PATH)
      message(FATAL_ERROR "genhtml not found! Aborting...")
    endif()

    # Targets
    add_custom_target(ccov-clean COMMAND ${LCOV_PATH} --directory
                                         ${CMAKE_BINARY_DIR} --zerocounters)

  else()
    message(FATAL_ERROR "Code coverage requires GCC. Aborting.")
  endif()
endif()

# Adds code coverage instrumentation to a library, or instrumentation/targets
# for an executable target.
# ~~~
# EXECUTABLE ADDED TARGETS:
# ccov : Generates HTML code coverage report for every target added with 'AUTO' parameter.

# Required:
# TARGET_NAME - Name of the target to generate code coverage for.
# Optional:
# PUBLIC - Sets the visibility for added compile options to targets to PUBLIC instead of the default of PRIVATE.
# PUBLIC - Sets the visibility for added compile options to targets to INTERFACE instead of the default of PRIVATE.
# AUTO - Adds the target to the 'ccov' target so that it can be run in a batch with others easily. Effective on executable targets.
# EXTERNAL - For GCC's lcov, allows the profiling of 'external' files from the processing directory
# COVERAGE_TARGET_NAME - For executables ONLY, changes the outgoing target name so instead of `ccov-${TARGET_NAME}` it becomes `ccov-${COVERAGE_TARGET_NAME}`.
# EXCLUDE <REGEX_PATTERNS> - Excludes files of the patterns provided from coverage. **These do not copy to the 'all' targets.**
# OBJECTS <TARGETS> - For executables ONLY, if the provided targets are shared libraries, adds coverage information to the output
# ARGS <ARGUMENTS> - For executables ONLY, appends the given arguments to the associated ccov-* executable call
# ~~~
function(crashdump_code_coverage TARGET_NAME)
  # Argument parsing
  set(options AUTO ALL EXTERNAL PUBLIC INTERFACE)
  set(single_value_keywords COVERAGE_TARGET_NAME)
  set(multi_value_keywords EXCLUDE OBJECTS ARGS)
  cmake_parse_arguments(
    crashdump_code_coverage "${options}" "${single_value_keywords}"
    "${multi_value_keywords}" ${ARGN})

  # Set the visibility of target functions to PUBLIC, INTERFACE or default to
  # PRIVATE.
  if(crashdump_code_coverage_PUBLIC)
    set(TARGET_VISIBILITY PUBLIC)
  elseif(crashdump_code_coverage_INTERFACE)
    set(TARGET_VISIBILITY INTERFACE)
  else()
    set(TARGET_VISIBILITY PRIVATE)
  endif()

  if(NOT crashdump_code_coverage_COVERAGE_TARGET_NAME)
    # If a specific name was given, use that instead.
    set(crashdump_code_coverage_COVERAGE_TARGET_NAME ${TARGET_NAME})
  endif()

  if(CODE_COVERAGE)
    # Add code coverage instrumentation to the target's linker command
    target_compile_options(${TARGET_NAME} ${TARGET_VISIBILITY} -fprofile-arcs
      -ftest-coverage)
    target_link_libraries(${TARGET_NAME} ${TARGET_VISIBILITY} gcov)

    # Targets
    get_target_property(target_type ${TARGET_NAME} TYPE)

    # For executables add targets to run and produce output
    if(target_type STREQUAL "EXECUTABLE")
      set(COVERAGE_INFO
          "${CODE_COVERAGE_OUTPUT_DIR}/${crashdump_code_coverage_COVERAGE_TARGET_NAME}.info"
      )

      # Run the executable, generating coverage information
      add_custom_target(
        ccov-run-${crashdump_code_coverage_COVERAGE_TARGET_NAME}
        COMMAND $<TARGET_FILE:${TARGET_NAME}> ${crashdump_code_coverage_ARGS}
        DEPENDS ccov-preprocessing ${TARGET_NAME})

      # Generate exclusion string for use
      foreach(EXCLUDE_ITEM ${crashdump_code_coverage_EXCLUDE})
        set(EXCLUDE_REGEX ${EXCLUDE_REGEX} --remove ${COVERAGE_INFO}
            '${EXCLUDE_ITEM}')
      endforeach()

      if(EXCLUDE_REGEX)
        set(EXCLUDE_COMMAND ${LCOV_PATH} ${EXCLUDE_REGEX} --output-file
                            ${COVERAGE_INFO})
      else()
        set(EXCLUDE_COMMAND ;)
      endif()

      if(NOT ${crashdump_code_coverage_EXTERNAL})
        set(EXTERNAL_OPTION --no-external)
      endif()

      # Capture coverage data
      add_custom_target(
        ccov-capture-${crashdump_code_coverage_COVERAGE_TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -E remove ${COVERAGE_INFO}
        COMMAND ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --zerocounters
        COMMAND $<TARGET_FILE:${TARGET_NAME}> ${crashdump_code_coverage_ARGS}
        COMMAND
          ${LCOV_PATH} --directory ${CMAKE_BINARY_DIR} --base-directory
          ${CMAKE_SOURCE_DIR} --capture ${EXTERNAL_OPTION} --output-file
          ${COVERAGE_INFO}
        COMMAND ${EXCLUDE_COMMAND}
        DEPENDS ccov-preprocessing ${TARGET_NAME})

      # Generates HTML output of the coverage information for perusal
      add_custom_target(
        ccov-${crashdump_code_coverage_COVERAGE_TARGET_NAME}
        COMMAND
          ${GENHTML_PATH} -o
          ${CODE_COVERAGE_OUTPUT_DIR}/${crashdump_code_coverage_COVERAGE_TARGET_NAME}
          ${COVERAGE_INFO}
        DEPENDS ccov-capture-${crashdump_code_coverage_COVERAGE_TARGET_NAME})
    endif()

    add_custom_command(
      TARGET ccov-${crashdump_code_coverage_COVERAGE_TARGET_NAME}
      POST_BUILD
      COMMAND ;
      COMMENT
        "Open ${CODE_COVERAGE_OUTPUT_DIR}/${crashdump_code_coverage_COVERAGE_TARGET_NAME}/index.html in your browser to view the coverage report."
    )

    # Add AUTO and normal targets to ccov
    if(NOT TARGET ccov)
      add_custom_target(ccov)
    endif()
    add_dependencies(ccov ccov-${crashdump_code_coverage_COVERAGE_TARGET_NAME})

  endif()
endfunction()