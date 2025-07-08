# Third party dependencies management

message(STATUS "Configuring third party dependencies...")

option(ENABLE_GLIB2 "Enable GLib2 support" ON)

# Add third party libraries
add_subdirectory(syscall_intercept)
add_subdirectory(Melon)

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

# 创建一个接口库来管理所有第三方依赖
add_library(third_party_deps INTERFACE)

# 配置基础的包含目录
target_include_directories(third_party_deps INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall_intercept/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Melon/include
)

# 配置基础的链接库
target_link_libraries(third_party_deps INTERFACE
    syscall_intercept_shared 
    melon_static
)

# 条件性添加 GLib2 的包含目录和链接库
if(ENABLE_GLIB2)
    target_include_directories(third_party_deps INTERFACE
        ${GLIB2_INCLUDE_DIRS}
        ${GOBJECT2_INCLUDE_DIRS}
        ${GIO2_INCLUDE_DIRS}
    )
    
    target_link_libraries(third_party_deps INTERFACE
        ${GLIB2_LIBRARIES}
        ${GOBJECT2_LIBRARIES}
        ${GIO2_LIBRARIES}
    )
    
    # 添加 GLib2 的编译标志
    target_compile_options(third_party_deps INTERFACE
        ${GLIB2_CFLAGS_OTHER}
        ${GOBJECT2_CFLAGS_OTHER}
        ${GIO2_CFLAGS_OTHER}
    )
    
    # 定义宏，让源代码知道 GLib2 可用
    target_compile_definitions(third_party_deps INTERFACE
        HAVE_GLIB2=1
    )
endif()

# 将变量传递给父作用域，供主 CMakeLists.txt 使用
set(THIRD_PARTY_CONFIGURED TRUE PARENT_SCOPE)
set(GLIB2_ENABLED ${ENABLE_GLIB2} PARENT_SCOPE)

message(STATUS "Third party dependencies configured:")
message(STATUS "  - syscall_intercept: Available")
message(STATUS "  - Melon: Available") 
message(STATUS "  - GLib2: ${ENABLE_GLIB2}")