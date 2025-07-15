@echo off
chcp 65001
echo Building VTK Qt Project...

REM Create build directory
if not exist build mkdir build
cd build

REM Clean previous CMake cache (if exists)
if exist CMakeCache.txt (
    echo Cleaning previous CMake cache...
    del CMakeCache.txt
)
if exist CMakeFiles (
    echo Cleaning CMakeFiles directory...
    rmdir /s /q CMakeFiles
)

REM Configure project
echo Configuring project...
cmake .. -G "Visual Studio 17 2022" -A x64

REM Check if configuration was successful
if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build project
echo Building project...
cmake --build . --config Release

REM Check if build was successful
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build completed!
echo Executable location: build\Exe\Release\VTK_Qt_Project.exe
pause 