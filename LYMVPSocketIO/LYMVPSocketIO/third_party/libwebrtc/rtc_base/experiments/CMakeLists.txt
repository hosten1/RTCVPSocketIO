add_library(rtc_base_experiments STATIC
   alr_experiment.cc
   filed_trial_params.cc
   field_trial_units.cc
   jitter_buffer_experiment.cc
)

# 明确列出所有头文件，不使用统配方式
set(HEADER_FILES
    alr_experiment.h
    field_trial_parser.h
    field_trial_units.h
    jitter_upper_bound_experiment.h
)

target_include_directories(rtc_base_experiments
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_experiments ARCHIVE DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/experiments)
