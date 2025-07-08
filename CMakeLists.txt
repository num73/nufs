cmake_minimum_required(VERSION 3.5)
project(nufs C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# 添加测试选项
option(BUILD_TESTS "Build unit tests with GLib2 GTest" ON)

# Add all third party dependencies (包含所有配置)
add_subdirectory(third)

# Define source files explicitly
set(NUFS_SOURCES
    src/shim.c
    src/util.c
    src/nufs.c
    # Add more source files here as needed
)

# Add the nufs shared library
add_library(nufs SHARED ${NUFS_SOURCES})

# 现在只需要链接第三方依赖接口库即可
target_link_libraries(nufs PRIVATE third_party_deps)

# Set output directory
set_target_properties(nufs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# ===== GTest 配置 =====
if(BUILD_TESTS)
    message(STATUS "Configuring GTest framework with GLib2 support")
    # 确保 GLib2 可用
    if(NOT GLIB2_ENABLED)
        message(FATAL_ERROR "GLib2 is required for testing but not enabled. Please enable GLib2 or disable tests.")
    endif()
    
    enable_testing()
    
    # 创建测试用的静态库（避免 LD_PRELOAD 问题）
    add_library(nufs_test_static STATIC ${NUFS_SOURCES})
    target_link_libraries(nufs_test_static PRIVATE third_party_deps)
    
    # 添加 gtest 子目录
    add_subdirectory(gtest)
    
    message(STATUS "GLib2 GTest framework configured successfully")
endif()

# Install targets
install(TARGETS nufs
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Optional: Create a simple test target
add_custom_target(nufs_usage_info
    COMMAND echo "To test nufs library, use: LD_PRELOAD=./lib/libnufs.so <your_program>"
    DEPENDS nufs
    COMMENT "Instructions for testing the nufs library"
)

# Print some build information
message(STATUS "=== ${PROJECT_NAME} Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "Third party configured: ${THIRD_PARTY_CONFIGURED}")
message(STATUS "GLib2 support: ${GLIB2_ENABLED}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Installing to: ${CMAKE_INSTALL_PREFIX}")