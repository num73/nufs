cmake_minimum_required(VERSION 3.5)
project(nufs C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Add the syscall_intercept subdirectory
add_subdirectory(third/syscall_intercept)
add_subdirectory(third/Melon)


# Define source files explicitly
set(NUFS_SOURCES
    src/shim.c
    src/util.c
    src/nufs.c
    # Add more source files here as needed
)

# Add the nufs shared library
add_library(nufs SHARED ${NUFS_SOURCES})

# 使用 target_* 命令来配置依赖（推荐方式）
target_include_directories(nufs PRIVATE
    third/syscall_intercept/include
    third/Melon/include
)

# Link nufs with syscall_intercept and Melon
target_link_libraries(nufs PRIVATE 
    syscall_intercept_shared 
    melon_static  # 使用 Melon 静态库
)

# Set output directory
set_target_properties(nufs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Install targets
install(TARGETS nufs
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Optional: Create a simple test target
add_custom_target(test_nufs
    COMMAND echo "To test nufs library, use: LD_PRELOAD=./lib/libnufs.so <your_program>"
    DEPENDS nufs
    COMMENT "Instructions for testing the nufs library"
)

# Print some build information
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C flags: ${CMAKE_C_FLAGS}")
message(STATUS "Installing to: ${CMAKE_INSTALL_PREFIX}")