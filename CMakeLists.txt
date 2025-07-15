cmake_minimum_required(VERSION 3.14)
project(NIFTI_Visualization_Library LANGUAGES CXX)

# 设置CUDA架构策略
cmake_policy(SET CMP0104 NEW)

# 生成编译数据库供IDE使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置CUDA标准
set(CMAKE_CUDA_STANDARD 11)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# 设置CUDA架构
set(CMAKE_CUDA_ARCHITECTURES 75)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Exe)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)

# Qt设置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/example)

# 设置Qt路径
set(Qt5_DIR "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/lib/cmake/Qt5")
set(CMAKE_PREFIX_PATH "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64")

# 查找Qt包
find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

# VTK配置 - 版本8.2.0
set(VTK_DIR "D:/code/vtk8.2.0/VTK-8.2.0/lib/cmake/vtk-8.2")
find_package(VTK 8.2 REQUIRED)

# CUDA配置
find_package(CUDAToolkit REQUIRED)

# VTK Qt支持检测和配置
if(VTK_QT_FOUND)
    message(STATUS "VTK Qt支持已找到")
else()
    set(VTK_QT_FOUND TRUE)
    message(STATUS "VTK Qt支持已启用")
endif()

# ========== 静态库部分 ==========

# 静态库源文件
set(LIBRARY_SOURCES
    lib/NiftiVisualizationAPI.cpp
    lib/niftimanager.cpp
    lib/brainregionvolume.cpp
)

# 静态库头文件
set(LIBRARY_HEADERS
    api/NiftiVisualizationAPI.h
    lib/niftimanager.h
    lib/brainregionvolume.h
)

# 创建静态库
add_library(NiftiVisualizationLib STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

# 静态库包含目录
target_include_directories(NiftiVisualizationLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 静态库链接库
target_link_libraries(NiftiVisualizationLib PUBLIC
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    ${VTK_LIBRARIES}
)

# 静态库编译定义
target_compile_definitions(NiftiVisualizationLib PRIVATE
    QT_DEPRECATED_WARNINGS
)

# 设置MSVC编译器UTF-8支持
if(MSVC)
    target_compile_options(NiftiVisualizationLib PRIVATE /utf-8)
endif()

# ========== 示例程序部分 ==========

# 示例程序源文件
set(EXAMPLE_SOURCES
    src/main.cpp
    example/mainwindow.cpp
)

# 示例程序头文件
set(EXAMPLE_HEADERS
    example/mainwindow.h
)

# 示例程序UI文件
set(EXAMPLE_FORMS
    example/mainwindow.ui
)

# 创建示例程序可执行文件
add_executable(${PROJECT_NAME}_Example ${EXAMPLE_SOURCES} ${EXAMPLE_HEADERS} ${EXAMPLE_FORMS})

# 示例程序包含目录
target_include_directories(${PROJECT_NAME}_Example PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/example
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 示例程序链接静态库
target_link_libraries(${PROJECT_NAME}_Example PRIVATE
    NiftiVisualizationLib
)

# 示例程序编译定义
target_compile_definitions(${PROJECT_NAME}_Example PRIVATE
    QT_DEPRECATED_WARNINGS
)

# 设置MSVC编译器UTF-8支持
if(MSVC)
    target_compile_options(${PROJECT_NAME}_Example PRIVATE /utf-8)
endif()

# ========== 部署配置 ==========

# 部署Qt DLL
if(WIN32)
    # 找到windeployqt.exe
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    
    # 添加命令在构建后运行windeployqt
    add_custom_command(TARGET ${PROJECT_NAME}_Example POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --verbose 0
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
            "$<TARGET_FILE:${PROJECT_NAME}_Example>"
        COMMENT "正在部署Qt..."
    )
endif()

# ========== 安装配置 ==========

# 安装静态库
install(TARGETS NiftiVisualizationLib
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装API头文件
install(FILES api/NiftiVisualizationAPI.h
    DESTINATION include
)

# 安装示例程序
install(TARGETS ${PROJECT_NAME}_Example
    RUNTIME DESTINATION bin
)