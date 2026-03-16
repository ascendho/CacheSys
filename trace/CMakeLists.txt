add_executable(cache_trace_compare
    trace_compare.cpp
)

target_include_directories(cache_trace_compare
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(cache_policy_scenarios
    policy_scenario_compare.cpp
)

target_include_directories(cache_policy_scenarios
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
