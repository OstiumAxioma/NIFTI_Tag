#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动生成VSCode/Cursor的C++配置文件
从CMakeLists.txt读取Qt和VTK路径，生成c_cpp_properties.json
"""

import os
import json
import re
from pathlib import Path

def extract_paths_from_cmake():
    """从CMakeLists.txt中提取Qt和VTK路径"""
    cmake_file = "CMakeLists.txt"
    if not os.path.exists(cmake_file):
        print(f"错误：找不到{cmake_file}文件")
        return None, None
    
    qt_path = None
    vtk_path = None
    
    with open(cmake_file, 'r', encoding='utf-8') as f:
        content = f.read()
        
        # 提取Qt路径
        qt_match = re.search(r'set\(Qt5_DIR\s+"([^"]+)"', content)
        if qt_match:
            qt_path = qt_match.group(1)
            # 转换为include路径
            qt_path = qt_path.replace('/lib/cmake/Qt5', '/include')
        
        # 提取VTK路径
        vtk_match = re.search(r'set\(VTK_DIR\s+"([^"]+)"', content)
        if vtk_match:
            vtk_path = vtk_match.group(1)
            # 转换为include路径
            vtk_path = vtk_path.replace('/lib/cmake/vtk-8.2', '/include/vtk-8.2')
    
    return qt_path, vtk_path

def find_compiler_path():
    """查找MSVC编译器路径"""
    possible_paths = [
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Professional/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
        "C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe",
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            return path
    
    # 如果找不到，尝试自动检测
    vs_base = "C:/Program Files/Microsoft Visual Studio/2022"
    if os.path.exists(vs_base):
        for edition in ["Community", "Professional", "Enterprise"]:
            edition_path = os.path.join(vs_base, edition)
            if os.path.exists(edition_path):
                msvc_path = os.path.join(edition_path, "VC/Tools/MSVC")
                if os.path.exists(msvc_path):
                    # 查找最新的MSVC版本
                    versions = [d for d in os.listdir(msvc_path) if os.path.isdir(os.path.join(msvc_path, d))]
                    if versions:
                        latest_version = sorted(versions)[-1]
                        compiler_path = os.path.join(msvc_path, latest_version, "bin/Hostx64/x64/cl.exe")
                        if os.path.exists(compiler_path):
                            return compiler_path
    
    return "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"

def generate_c_cpp_properties(qt_path, vtk_path, compiler_path):
    """生成c_cpp_properties.json配置"""
    
    # 基础include路径
    include_paths = [
        "${workspaceFolder}/header",
        "${workspaceFolder}/src", 
        "${workspaceFolder}/build"
    ]
    
    # 添加Qt路径
    if qt_path and os.path.exists(qt_path):
        include_paths.extend([
            qt_path,
            f"{qt_path}/QtCore",
            f"{qt_path}/QtGui", 
            f"{qt_path}/QtWidgets",
            f"{qt_path}/QtOpenGL"
        ])
        print(f"✓ 找到Qt路径: {qt_path}")
    else:
        print("⚠ 未找到Qt路径，使用默认路径")
        include_paths.extend([
            "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include",
            "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include/QtCore",
            "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include/QtGui",
            "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include/QtWidgets",
            "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include/QtOpenGL"
        ])
    
    # 添加VTK路径
    if vtk_path and os.path.exists(vtk_path):
        include_paths.extend([
            vtk_path,
            f"{vtk_path}/vtkRenderingOpenGL2",
            f"{vtk_path}/vtkGUISupportQt",
            f"{vtk_path}/vtkGUISupportQtOpenGL"
        ])
        print(f"✓ 找到VTK路径: {vtk_path}")
    else:
        print("⚠ 未找到VTK路径，使用默认路径")
        include_paths.extend([
            "D:/code/vtk8.2.0/VTK-8.2.0/include/vtk-8.2",
            "D:/code/vtk8.2.0/VTK-8.2.0/include/vtk-8.2/vtkRenderingOpenGL2",
            "D:/code/vtk8.2.0/VTK-8.2.0/include/vtk-8.2/vtkGUISupportQt",
            "D:/code/vtk8.2.0/VTK-8.2.0/include/vtk-8.2/vtkGUISupportQtOpenGL"
        ])
    
    # 生成配置
    config = {
        "configurations": [
            {
                "name": "Win32",
                "includePath": include_paths,
                "defines": [
                    "UNICODE",
                    "_UNICODE", 
                    "WIN32",
                    "_WIN32",
                    "WIN32_LEAN_AND_MEAN",
                    "NOMINMAX",
                    "QT_DEPRECATED_WARNINGS",
                    "VTK_QT_FOUND"
                ],
                "compilerPath": compiler_path,
                "cStandard": "c11",
                "cppStandard": "c++11",
                "intelliSenseMode": "windows-msvc-x64",
                "compilerArgs": [
                    "/utf-8"
                ],
                "browse": {
                    "path": [
                        "${workspaceFolder}",
                        qt_path or "C:/Qt/Qt5.12.9/5.12.9/msvc2017_64/include",
                        vtk_path or "D:/code/vtk8.2.0/VTK-8.2.0/include"
                    ],
                    "limitSymbolsToIncludedHeaders": True,
                    "databaseFilename": "${workspaceFolder}/.vscode/browse.vc.db"
                }
            }
        ],
        "version": 4
    }
    
    return config

def main():
    """主函数"""
    print("🔧 自动生成VSCode/Cursor C++配置文件")
    print("=" * 50)
    
    # 创建.vscode目录
    vscode_dir = Path(".vscode")
    vscode_dir.mkdir(exist_ok=True)
    
    # 提取路径
    qt_path, vtk_path = extract_paths_from_cmake()
    
    # 查找编译器
    compiler_path = find_compiler_path()
    print(f"✓ 编译器路径: {compiler_path}")
    
    # 生成配置
    config = generate_c_cpp_properties(qt_path, vtk_path, compiler_path)
    
    # 写入文件
    output_file = vscode_dir / "c_cpp_properties.json"
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=4, ensure_ascii=False)
    
    print(f"✓ 配置文件已生成: {output_file}")
    print("\n📝 下一步操作:")
    print("1. 重启Cursor/VSCode编辑器")
    print("2. 等待IntelliSense重新加载")
    print("3. 检查头文件是否不再报错")
    
    # 验证生成的配置
    print(f"\n🔍 验证配置:")
    print(f"  - Qt路径: {qt_path or '使用默认路径'}")
    print(f"  - VTK路径: {vtk_path or '使用默认路径'}")
    print(f"  - 编译器: {compiler_path}")
    print(f"  - Include路径数量: {len(config['configurations'][0]['includePath'])}")

if __name__ == "__main__":
    main() 