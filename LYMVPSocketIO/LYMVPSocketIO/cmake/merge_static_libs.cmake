# merge_static_libs.cmake
# Merge multiple static libraries into one using libtool (Apple platforms)
#
# Input variables:
#   FRAMEWORK_BINARY - path to the output framework binary
#   Any variable ending with _LIB will be treated as an input library path

cmake_minimum_required(VERSION 3.16)

# Collect all input libraries (any variable ending with _LIB)
set(INPUT_LIBS "")

# Add the original framework binary first
if(DEFINED FRAMEWORK_BINARY AND EXISTS "${FRAMEWORK_BINARY}")
    list(APPEND INPUT_LIBS "${FRAMEWORK_BINARY}")
endif()

# Get all variables ending with _LIB
get_cmake_property(_vars VARIABLES)
foreach(_var ${_vars})
    if(_var MATCHES "^.+_LIB$")
        set(_lib_path "${${_var}}")
        if(EXISTS "${_lib_path}" AND IS_ABSOLUTE "${_lib_path}")
            list(APPEND INPUT_LIBS "${_lib_path}")
        endif()
    endif()
endforeach()

# Remove duplicates
list(REMOVE_DUPLICATES INPUT_LIBS)

message(STATUS "========================================")
message(STATUS "Merging static libraries into framework:")
message(STATUS "  Output: ${FRAMEWORK_BINARY}")
message(STATUS "  Input libraries:")
foreach(lib ${INPUT_LIBS})
    get_filename_component(lib_name "${lib}" NAME)
    message(STATUS "    - ${lib_name}")
endforeach()
message(STATUS "========================================")

# Find libtool
find_program(LIBTOOL_EXECUTABLE NAMES libtool PATHS /usr/bin /usr/local/bin)
if(NOT LIBTOOL_EXECUTABLE)
    message(FATAL_ERROR "libtool not found!")
endif()

# Temporary output file
set(TEMP_OUTPUT "${FRAMEWORK_BINARY}.merged")

# Run libtool to merge all static libraries
message(STATUS "Running libtool merge...")
execute_process(
    COMMAND "${LIBTOOL_EXECUTABLE}" -static -o "${TEMP_OUTPUT}" ${INPUT_LIBS}
    RESULT_VARIABLE MERGE_RESULT
    OUTPUT_VARIABLE MERGE_OUTPUT
    ERROR_VARIABLE MERGE_ERROR
)

if(MERGE_RESULT EQUAL 0)
    # Replace original framework binary with merged one
    file(RENAME "${TEMP_OUTPUT}" "${FRAMEWORK_BINARY}")
    message(STATUS "✅ Successfully merged all libraries!")
    
    # Print size info
    execute_process(
        COMMAND du -sh "${FRAMEWORK_BINARY}"
        OUTPUT_VARIABLE SIZE_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Merged library size: ${SIZE_OUTPUT}")
    
    # Count object files
    execute_process(
        COMMAND ar -t "${FRAMEWORK_BINARY}"
        OUTPUT_VARIABLE AR_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX MATCHALL "\n" NEWLINE_MATCHES "${AR_OUTPUT}")
    list(LENGTH NEWLINE_MATCHES OBJ_COUNT)
    math(EXPR OBJ_COUNT "${OBJ_COUNT} + 1")
    message(STATUS "Total object files: ~${OBJ_COUNT}")
    message(STATUS "========================================")
else()
    message(FATAL_ERROR "❌ Failed to merge static libraries:\n${MERGE_ERROR}")
endif()
