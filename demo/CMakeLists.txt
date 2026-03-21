add_executable(cache_demo
    main.cpp
    common.cpp
    scenarios.cpp
)

target_include_directories(cache_demo
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(cache_demo PRIVATE cxx_std_17)
