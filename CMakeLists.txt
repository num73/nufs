cmake_minimum_required(VERSION 3.5)
project(nufs C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Add the syscall_intercept subdirectory
add_subdirectory(third/syscall_intercept)

# Include directories
include_directories(third/syscall_intercept/include)

# Define source files explicitly
set(NUFS_SOURCES
    src/shim.c
    src/util.c
    # Add more source files here as needed
)
# Add the nufs shared library
add_library(nufs SHARED ${NUFS_SOURCES})

# Link nufs with syscall_intercept
target_link_libraries(nufs PRIVATE syscall_intercept_shared)

# Set output directory
set_target_properties(nufs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Install targets
install(TARGETS nufs
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Optional: Create a simple test target that can be used to verify the library works
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
