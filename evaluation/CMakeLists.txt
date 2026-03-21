add_executable(cache_trace_compare
    trace/compare.cpp
    trace/generators.cpp
    trace/simulators.cpp
    trace/report.cpp
)

target_include_directories(cache_trace_compare
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

add_executable(cache_policy_scenarios
    scenario/compare.cpp
    scenario/generators.cpp
    scenario/evaluator.cpp
    scenario/report.cpp
)

target_include_directories(cache_policy_scenarios
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)
