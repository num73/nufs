cmake_minimum_required(VERSION 3.5)
project(nufs C ASM)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# 添加控制 GLib2 的选项
option(ENABLE_GLIB2 "Enable GLib2 support" ON)

# 条件性查找 GLib2
if(ENABLE_GLIB2)
    # Find pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB2 REQUIRED glib-2.0)
    pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
    pkg_check_modules(GIO2 REQUIRED gio-2.0)
    
    message(STATUS "GLib2 support enabled")
    message(STATUS "GLib2 version: ${GLIB2_VERSION}")
else()
    message(STATUS "GLib2 support disabled")
endif()

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

# 基础的包含目录
target_include_directories(nufs PRIVATE
    third/syscall_intercept/include
    third/Melon/include
)

# 条件性添加 GLib2 的包含目录
if(ENABLE_GLIB2)
    target_include_directories(nufs PRIVATE
        ${GLIB2_INCLUDE_DIRS}
        ${GOBJECT2_INCLUDE_DIRS}
        ${GIO2_INCLUDE_DIRS}
    )
endif()

# 基础的链接库
target_link_libraries(nufs PRIVATE 
    syscall_intercept_shared 
    melon_static  # 使用 Melon 静态库
)

# 条件性链接 GLib2
if(ENABLE_GLIB2)
    target_link_libraries(nufs PRIVATE
        ${GLIB2_LIBRARIES}
        ${GOBJECT2_LIBRARIES}
        ${GIO2_LIBRARIES}
    )
    
    # 添加 GLib2 的编译标志
    target_compile_options(nufs PRIVATE
        ${GLIB2_CFLAGS_OTHER}
        ${GOBJECT2_CFLAGS_OTHER}
        ${GIO2_CFLAGS_OTHER}
    )
    
    # 定义宏，让源代码知道 GLib2 可用
    target_compile_definitions(nufs PRIVATE
        HAVE_GLIB2=1
    )
endif()

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
message(STATUS "GLib2 support: ${ENABLE_GLIB2}")
message(STATUS "Installing to: ${CMAKE_INSTALL_PREFIX}")