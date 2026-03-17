# Third party dependencies management

message(STATUS "Configuring third party dependencies...")

# Add third party libraries
add_subdirectory(syscall_intercept)
add_subdirectory(Melon)

# 创建一个接口库来管理所有第三方依赖
add_library(third_party_deps INTERFACE)

# 配置包含目录
target_include_directories(third_party_deps INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/syscall_intercept/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Melon/include
)

# 配置链接库
target_link_libraries(third_party_deps INTERFACE
    syscall_intercept_shared 
    melon_static
)

# 将变量传递给父作用域，供主 CMakeLists.txt 使用
set(THIRD_PARTY_CONFIGURED TRUE PARENT_SCOPE)

message(STATUS "Third party dependencies configured:")
message(STATUS "  - syscall_intercept: Available")
message(STATUS "  - Melon: Available")