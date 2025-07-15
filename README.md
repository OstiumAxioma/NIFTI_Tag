# NIFTI脑影像可视化静态库

这是一个基于VTK 8.2.0和Qt 5.12.9的NIFTI脑影像与脑区标签可视化**静态库**项目。

## 项目定位

**重要：本项目是一个静态库，不是独立应用程序！**

- 🏗️ **静态库设计**：整个项目编译为静态库，供团队成员在其他项目中使用
- 🎯 **单一API接口**：所有功能通过唯一的头文件 `NiftiVisualizationAPI.h` 提供
- 📝 **示例程序**：MainWindow仅作为API调用示例，展示如何使用静态库接口
- 🔒 **功能封装**：所有NIFTI处理逻辑封装在库内部，外部只需调用API

## 项目结构

```
NIFTI_Tag/
├── CMakeLists.txt                    # 静态库编译配置
├── build.bat                         # 构建脚本
├── README.md                         # 项目说明
├── api/                              # 静态库API接口
│   └── NiftiVisualizationAPI.h       # 唯一对外接口头文件
├── lib/                              # 静态库核心实现
│   ├── NiftiVisualizationAPI.cpp     # API实现
│   ├── niftimanager.h
│   ├── niftimanager.cpp
│   ├── brainregionvolume.h
│   └── brainregionvolume.cpp
├── example/                          # 使用示例（MainWindow）
│   ├── mainwindow.h
│   ├── mainwindow.cpp
│   └── mainwindow.ui
├── src/
│   └── main.cpp                      # 示例程序入口
├── build/                            # 编译输出目录
│   ├── Lib/Release/
│   │   └── NiftiVisualizationLib.lib # 编译后的静态库文件
│   └── Exe/Release/
│       └── NIFTI_Visualization_Library_Example.exe # 编译后的示例程序
└── reference/                        # 参考资料
```

### 编译后文件位置

**1. 静态库文件位置**
```
build/Lib/Release/NiftiVisualizationLib.lib
```
- 这是编译后生成的静态库文件（约260KB）
- 团队成员使用时需要链接此文件

**2. build.bat脚本作用**
```bash
.\build.bat
```
- 自动清理之前的编译缓存
- 配置CMake项目（使用Visual Studio 2022 x64）
- 编译静态库和示例程序
- 部署Qt依赖文件
- 一键完成整个构建过程

**3. 示例程序位置**
```
build/Exe/Release/NIFTI_Visualization_Library_Example.exe
```
- 这是编译后的示例程序（约105KB）
- 展示如何使用静态库API
- 包含完整的VTK渲染环境和Qt界面

**4. API接口位置**
```
api/NiftiVisualizationAPI.h
```
- 唯一的对外接口头文件
- 团队成员只需要包含此文件
- 提供完整的NIFTI可视化功能接口

## 静态库使用方法

### 1. 编译静态库
```bash
build.bat
```
编译后生成：
- `lib/libNiftiVisualization.a` (或 `.lib`)
- `api/NiftiVisualizationAPI.h`

### 2. 在项目中使用
```cpp
#include "NiftiVisualizationAPI.h"

// 创建可视化实例
NiftiVisualizationAPI* niftiViz = new NiftiVisualizationAPI();

// 设置VTK渲染器
niftiViz->setRenderer(yourRenderer);

// 加载NIFTI文件
niftiViz->loadMriNifti("path/to/mri.nii");
niftiViz->loadLabelNifti("path/to/labels.nii");

// 处理和可视化
niftiViz->processRegions();

// 控制区块显示
niftiViz->setRegionVisibility(33, true);
niftiViz->sortVolumesByCamera();
```

## 静态库功能特性

- **NIFTI文件处理**：
  - 支持MRI NIFTI文件的灰度体绘制
  - 支持脑区标签NIFTI文件的分区着色
  - 标签与MRI融合显示：同一区块染色，保留灰度细节

- **体绘制管理**：
  - 每个标签区块独立的vtkVolume对象
  - 多标签区块独立显示/隐藏控制
  - 自动渲染顺序调整，避免VTK 8.2多体渲染bug

- **API接口**：
  - 简洁的C++接口，易于集成
  - 完整的错误处理和状态回调
  - 线程安全的设计

## 静态库API设计

### 核心类：NiftiVisualizationAPI

```cpp
class NiftiVisualizationAPI {
public:
    // 构造与析构
    NiftiVisualizationAPI();
    ~NiftiVisualizationAPI();
    
    // 渲染器设置
    void setRenderer(vtkRenderer* renderer);
    
    // 文件加载
    bool loadMriNifti(const QString& filePath);
    bool loadLabelNifti(const QString& filePath);
    
    // 数据处理
    void processRegions();
    void clearRegions();
    
    // 区块控制
    void setRegionVisibility(int label, bool visible);
    void setAllRegionsVisibility(bool visible);
    void sortVolumesByCamera();
    
    // 信息获取
    QList<int> getAllLabels() const;
    QColor getRegionColor(int label) const;
    bool isRegionVisible(int label) const;
    
    // 状态查询
    bool hasMriData() const;
    bool hasLabelData() const;
    int getRegionCount() const;
    
    // 回调设置
    void setErrorCallback(std::function<void(const QString&)> callback);
    void setRegionsProcessedCallback(std::function<void()> callback);
    void setRegionVisibilityCallback(std::function<void(int, bool)> callback);
};
```

## 示例程序说明

`example/mainwindow.*` 仅作为API使用示例，展示：
- 如何创建和配置NiftiVisualizationAPI实例
- 如何处理文件加载和错误
- 如何实现UI控制和状态同步
- 如何集成到VTK渲染管线

**重要：示例程序不包含任何核心功能实现，所有功能都通过API调用！**

## 开发规则

### 最高优先级规则
1. **静态库优先**：所有功能必须通过静态库API提供
2. **单一接口**：外部只能访问 `NiftiVisualizationAPI.h`
3. **示例分离**：MainWindow只能调用API，不能直接访问内部类
4. **功能封装**：所有NIFTI处理逻辑封装在lib/目录内

### 架构约束
- MainWindow不能include任何lib/目录下的头文件
- 所有VTK操作必须通过API封装
- 静态库必须是线程安全的
- API接口必须简洁、易用、稳定

## 编译要求

- **VTK**: 8.2.0
- **Qt**: 5.12.9
- **编译器**: Visual Studio 2022 或更高版本
- **CMake**: 3.14 或更高版本

## 许可证

本项目仅供学习和研究使用。 