add_library(rtc_base_experiments STATIC
   alr_experiment.cc
   filed_trial_params.cc
   field_trial_units.cc
   jitter_buffer_experiment.cc
)

target_include_directories(rtc_base_experiments
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

install(TARGETS rtc_base_experiments ARCHIVE DESTINATION lib)
install(DIRECTORY . DESTINATION include/rtc_base/experiments FILES_MATCHING PATTERN "*.h")
