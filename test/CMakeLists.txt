include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(cache_tests
    lru_lfu_test.cpp
)

target_include_directories(cache_tests
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(cache_tests
    PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cache_tests)
