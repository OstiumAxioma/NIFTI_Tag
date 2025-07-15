# VTK Qt 项目

这是一个基于VTK 8.2.0和Qt 5.12.9的3D可视化项目模板。

## 项目结构

```
PointCloud_Crop/
├── CMakeLists.txt          # CMake配置文件
├── build.bat              # Windows构建脚本
├── README.md              # 项目说明
├── header/                # 头文件目录
│   └── mainwindow.h       # 主窗口头文件
└── src/                   # 源文件目录
    ├── main.cpp           # 主程序入口
    ├── mainwindow.cpp     # 主窗口实现
    └── mainwindow.ui      # Qt Designer UI文件
```

## 环境要求

- **VTK**: 8.2.0 (安装在 `D:\Compile\VTK\VTK-Qt\install\VTK820`)
- **Qt**: 5.12.9 (安装在 `C:\Qt\Qt5.12.9\5.12.9\msvc2017_64`)
- **编译器**: Visual Studio 2022 或更高版本
- **CMake**: 3.14 或更高版本

## 构建步骤

### 方法1: 使用构建脚本 (推荐)
```bash
# 在项目根目录运行
build.bat
```

### 方法2: 手动构建
```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake .. -G "Visual Studio 17 2022" -A x64

# 构建项目
cmake --build . --config Release
```

## 功能特性

- **3D可视化**: 使用VTK进行3D图形渲染
- **Qt界面**: 现代化的Qt用户界面
- **交互式操作**: 支持鼠标交互和3D场景操作
- **菜单系统**: 包含文件和帮助菜单
- **工具栏**: 提供快速操作按钮
- **状态栏**: 显示操作状态信息

## 默认功能

项目启动时会自动显示一个3D球体，您可以通过以下方式与程序交互：

- **鼠标操作**: 
  - 左键拖拽：旋转视角
  - 中键拖拽：平移视角
  - 滚轮：缩放

- **菜单操作**:
  - 文件 → 退出：关闭程序
  - 帮助 → 关于：显示关于信息

- **工具栏操作**:
  - 创建球体：在场景中添加新的球体

## 自定义开发

### 添加新的VTK对象
在 `mainwindow.cpp` 中添加新的方法来创建不同的VTK对象，例如：

```cpp
void MainWindow::createCube()
{
    // 创建立方体源
    auto cubeSource = vtkSmartPointer<vtkCubeSource>::New();
    
    // 创建映射器
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(cubeSource->GetOutputPort());
    
    // 创建演员
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    
    // 添加到渲染器
    renderer->AddActor(actor);
    renderWindow->Render();
}
```

### 修改UI界面
使用Qt Designer编辑 `src/mainwindow.ui` 文件来自定义界面布局。

## 故障排除

### 常见问题

1. **VTK路径错误**: 确保VTK_DIR路径正确指向您的VTK安装目录
2. **Qt路径错误**: 确保Qt5_DIR路径正确指向您的Qt安装目录
3. **编译器版本**: 确保使用与Qt和VTK兼容的编译器版本

### 调试信息
如果构建失败，请检查：
- CMake错误信息
- 编译器错误信息
- 路径配置是否正确

## 许可证

本项目仅供学习和研究使用。 