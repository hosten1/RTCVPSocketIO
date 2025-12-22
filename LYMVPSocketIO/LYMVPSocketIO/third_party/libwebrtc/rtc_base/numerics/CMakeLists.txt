# numerics只有头文件，不构建库

# 明确列出需要安装的头文件
set(HEADER_FILES
    math_utils.h
    mod_ops.h
    moving_median_filter.h
    percentile_filter.h
    running_statistics.h
    safe_compare.h
    safe_conversions.h
    safe_conversions_impl.h
    safe_minmax.h
)

# 明确列出需要安装的头文件，不使用统配方式
install(FILES ${HEADER_FILES} DESTINATION include/rtc_base/numerics)