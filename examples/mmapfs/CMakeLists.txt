# examples/mmapfs/CMakeLists.txt - Build configuration for the mmapfs example

message(STATUS "Configuring mmapfs example filesystem...")

# Source files for the mmapfs filesystem implementation
set(MMAPFS_SOURCES
    device.c
    alloc.c
    inode.c
    dir.c
    journal.c
    nufs_ops.c
)

# Build shared library (used with LD_PRELOAD, replaces libnufs.so)
add_library(nufs_mmapfs SHARED
    ${MMAPFS_SOURCES}
    ${CMAKE_SOURCE_DIR}/src/shim.c
    ${CMAKE_SOURCE_DIR}/src/util.c
)

target_include_directories(nufs_mmapfs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(nufs_mmapfs PRIVATE
    third_party_deps
    pthread
)

set_target_properties(nufs_mmapfs PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Build the standalone mkfs formatting tool
add_executable(mkfs_mmapfs mkfs.c)

target_include_directories(mkfs_mmapfs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(mkfs_mmapfs PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

message(STATUS "  + Library : nufs_mmapfs  (LD_PRELOAD=lib/libnufs_mmapfs.so)")
message(STATUS "  + Tool    : mkfs_mmapfs  (bin/mkfs_mmapfs)")
