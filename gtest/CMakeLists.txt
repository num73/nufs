# gtest/CMakeLists.txt - 测试框架配置

message(STATUS "Configuring test framework...")

# 测试源文件
set(TEST_SOURCES
    test_util.c
)

# 为每个测试文件创建可执行文件
foreach(test_source ${TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    
    # 创建测试可执行文件
    add_executable(${test_name} ${test_source})
    
    # 链接测试库和依赖
    target_link_libraries(${test_name} PRIVATE
        nufs_test_static
        third_party_deps
    )
    
    # 添加包含目录
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/gtest
    )
    
    # 添加测试到 CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
    
    # 设置测试超时
    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
    )
    
    message(STATUS "  + Test: ${test_name}")
endforeach()


# 创建便捷的测试目标
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${TEST_SOURCES}
    COMMENT "Running all tests"
)

message(STATUS "Test framework configured with ${CMAKE_CURRENT_SOURCE_DIR}")