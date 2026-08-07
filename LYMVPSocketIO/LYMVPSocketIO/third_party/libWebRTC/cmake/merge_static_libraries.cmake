# merge_static_libraries.cmake
#
# 通用静态库合并函数，支持 Apple (libtool) 和 Linux/Unix (ar MRI) 平台
#
# 使用方式:
#   include(merge_static_libraries)
#   merge_static_libraries(webrtc_merged "rtc_base;api;system_wrappers")
#   target_link_libraries(my_target PRIVATE webrtc_merged)
#
# 参数:
#   OUTPUT_TARGET   - 输出的合并库目标名
#   INPUT_TARGETS   - 要合并的输入目标列表（用分号分隔）

function(merge_static_libraries OUTPUT_TARGET INPUT_TARGETS)
    set(LIB_FILES "")
    foreach(TGT ${INPUT_TARGETS})
        if(TARGET ${TGT})
            list(APPEND LIB_FILES "$<TARGET_FILE:${TGT}>")
        else()
            message(WARNING "merge_static_libraries: target '${TGT}' not found, skipping")
        endif()
    endforeach()

    if(NOT LIB_FILES)
        message(FATAL_ERROR "merge_static_libraries: no valid input targets found")
    endif()

    set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/lib${OUTPUT_TARGET}.a")

    if(APPLE)
        # macOS/iOS: 使用 Apple 的 libtool -static
        add_custom_command(
            OUTPUT "${OUTPUT_FILE}"
            COMMAND libtool -static -o "${OUTPUT_FILE}" ${LIB_FILES}
            DEPENDS ${INPUT_TARGETS}
            COMMENT "Merging static libraries into lib${OUTPUT_TARGET}.a (libtool)"
            VERBATIM
        )
    elseif(UNIX)
        # Linux/Unix: 使用 ar 的 MRI 脚本模式
        set(MRI_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/merge_${OUTPUT_TARGET}.mri")
        
        # 使用 file(GENERATE) 生成 MRI 脚本，支持生成器表达式
        set(MRI_CONTENT "CREATE lib${OUTPUT_TARGET}.a\n")
        foreach(TGT ${INPUT_TARGETS})
            if(TARGET ${TGT})
                set(MRI_CONTENT "${MRI_CONTENT}ADDLIB $<TARGET_FILE:${TGT}>\n")
            endif()
        endforeach()
        set(MRI_CONTENT "${MRI_CONTENT}SAVE\nEND\n")
        
        file(GENERATE
            OUTPUT "${MRI_SCRIPT}"
            CONTENT "${MRI_CONTENT}"
        )

        add_custom_command(
            OUTPUT "${OUTPUT_FILE}"
            COMMAND ar -M < "${MRI_SCRIPT}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS ${INPUT_TARGETS} "${MRI_SCRIPT}"
            COMMENT "Merging static libraries into lib${OUTPUT_TARGET}.a (ar MRI)"
            VERBATIM
        )
    elseif(MSVC)
        # Windows: 使用 lib.exe
        set(LIB_COMMAND "lib.exe /OUT:\"${OUTPUT_FILE}\"")
        foreach(lib ${LIB_FILES})
            set(LIB_COMMAND "${LIB_COMMAND} \"${lib}\"")
        endforeach()
        
        add_custom_command(
            OUTPUT "${OUTPUT_FILE}"
            COMMAND ${LIB_COMMAND}
            DEPENDS ${INPUT_TARGETS}
            COMMENT "Merging static libraries into lib${OUTPUT_TARGET}.a (lib.exe)"
            VERBATIM
        )
    else()
        message(FATAL_ERROR "merge_static_libraries: unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    # 创建一个自定义目标来驱动合并
    add_custom_target("${OUTPUT_TARGET}_merge"
        DEPENDS "${OUTPUT_FILE}"
    )

    # 创建 IMPORTED 库目标，方便其他 target 链接
    add_library("${OUTPUT_TARGET}" STATIC IMPORTED GLOBAL)
    set_target_properties("${OUTPUT_TARGET}" PROPERTIES
        IMPORTED_LOCATION "${OUTPUT_FILE}"
    )
    add_dependencies("${OUTPUT_TARGET}" "${OUTPUT_TARGET}_merge")

    # 继承第一个输入目标的 include 目录（如果有的话）
    list(GET INPUT_TARGETS 0 FIRST_TARGET)
    if(TARGET ${FIRST_TARGET})
        get_target_property(_inc_dirs ${FIRST_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
        if(_inc_dirs)
            set_target_properties("${OUTPUT_TARGET}" PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${_inc_dirs}"
            )
        endif()
    endif()

    message(STATUS "merge_static_libraries: will merge ${OUTPUT_TARGET} from: ${INPUT_TARGETS}")
endfunction()
