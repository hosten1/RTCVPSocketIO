# merge_static_libs_simple.cmake
# 简单的静态库合并脚本，接收空格分隔的库路径列表
# 输入:
#   FRAMEWORK_BINARY - 输出的 framework 二进制文件路径
#   MERGE_LIBS - 空格分隔的输入库路径列表

cmake_minimum_required(VERSION 3.16)

message(STATUS "========================================")
message(STATUS "Merging static libraries into framework:")
message(STATUS "  Output: ${FRAMEWORK_BINARY}")

# 把 MERGE_LIBS 拆分成列表
separate_arguments(INPUT_LIBS UNIX_COMMAND "${MERGE_LIBS}")

# 把 framework 自己也加进去
list(INSERT INPUT_LIBS 0 "${FRAMEWORK_BINARY}")

message(STATUS "  Input libraries:")
foreach(lib ${INPUT_LIBS})
    if(EXISTS "${lib}")
        get_filename_component(lib_name "${lib}" NAME)
        message(STATUS "    ✓ ${lib_name}")
    else()
        get_filename_component(lib_name "${lib}" NAME)
        message(STATUS "    ✗ ${lib_name} (NOT FOUND)")
    endif()
endforeach()

# 只保留存在的库
set(EXISTING_LIBS "")
foreach(lib ${INPUT_LIBS})
    if(EXISTS "${lib}")
        list(APPEND EXISTING_LIBS "${lib}")
    endif()
endforeach()

message(STATUS "========================================")

if(NOT EXISTING_LIBS)
    message(FATAL_ERROR "No valid input libraries found!")
endif()

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
    COMMAND "${LIBTOOL_EXECUTABLE}" -static -o "${TEMP_OUTPUT}" ${EXISTING_LIBS}
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
