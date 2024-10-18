cmake_minimum_required(VERSION 3.5)

# 设置项目名称和版本
project(client_gui VERSION 1.0 LANGUAGES CXX)

# 查找 Qt6 的组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

aux_source_directory(./src srcs)

# 添加可执行文件
add_executable(${PROJECT_NAME} 
    ${srcs} 
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "client")

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy
#     $<TARGET_FILE:${PROJECT_NAME}> /client
# )

# 链接 Qt6 的库到可执行文件
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)

# 为项目启用 C++17 标准（根据需要可以修改）
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# 如果需要，可以设置构建类型
# set(CMAKE_BUILD_TYPE Release)

# 配置 Qt 的自动生成工具（如 MOC、UIC 和 RCC）
# qt_standard_project_setup()

# 为了在 Windows 上打包应用程序时添加 Qt 依赖，可以使用以下命令
# find_package(Qt6 COMPONENTS WinMain REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::WinMain)
