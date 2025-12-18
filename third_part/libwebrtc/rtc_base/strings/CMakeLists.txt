add_library(rtc_base_strings STATIC
   string_builder.cc
   string_builder.h
   string_format.cc
   string_format.h
)
target_include_directories(rtc_base_strings
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>
)
install(DIRECTORY include/ DESTINATION include/rtc_base/strings)
