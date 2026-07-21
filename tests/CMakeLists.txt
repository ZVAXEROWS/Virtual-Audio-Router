# ===========================================================================
# tests/CMakeLists.txt
# ===========================================================================

enable_testing()

# ---------------------------------------------------------------------------
# Helper macro: create a GTest executable
# ---------------------------------------------------------------------------
macro(var_add_test TEST_NAME)
    add_executable(${TEST_NAME} ${ARGN})
    target_link_libraries(${TEST_NAME}
        PRIVATE
            var_engine_lib
            GTest::gtest_main
    )
    target_include_directories(${TEST_NAME}
        PRIVATE
            "${CMAKE_SOURCE_DIR}/engine/include"
            "${CMAKE_SOURCE_DIR}/core/include"
            "${CMAKE_BINARY_DIR}/include"
    )
    include(GoogleTest)
    gtest_discover_tests(${TEST_NAME})
endmacro()

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------

var_add_test(test_result    unit/test_result.cpp)
var_add_test(test_logger    unit/test_logger.cpp)
