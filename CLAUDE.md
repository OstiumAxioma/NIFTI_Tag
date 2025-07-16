# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**NIFTI Brain Imaging Visualization Static Library** - A VTK 8.2.0 and Qt 5.12.9 based static library for visualizing NIFTI brain images with region labels.

**Critical Architecture Rules**:
- This is a **static library**, NOT a standalone application
- All functionality exposed through single API: `api/NiftiVisualizationAPI.h`
- MainWindow is ONLY an example - cannot access internal library components
- All core functionality must reside in `lib/` directory

## Build Commands

```bash
# Build everything (library + example)
.\build.bat

# Clean build
rmdir /s /q build
.\build.bat

# Manual CMake build
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Build outputs**:
- Static library: `build/Lib/Release/NiftiVisualizationLib.lib`
- Example app: `build/Exe/Release/NIFTI_Visualization_Library_Example.exe`

## Project Architecture

### Directory Structure
```
NIFTI_Tag/
├── api/                    # Public API (single header)
│   └── NiftiVisualizationAPI.h
├── lib/                    # Core implementation (private)
│   ├── NiftiVisualizationAPI.cpp
│   ├── niftimanager.h/cpp
│   ├── brainregionvolume.h/cpp
│   └── MultiResolutionNiftiProcessor.h
├── example/                # Usage example ONLY
│   └── mainwindow.h/cpp/ui
└── src/main.cpp           # Example entry point
```

### Data Flow Architecture
1. **API Layer** → **NiftiManager** → **BrainRegionVolume**
2. Two NIFTI inputs (same size: 182×218×182):
   - MRI NIFTI: Brain matter data
   - Label NIFTI: Region labels (imprecise)
3. Each label becomes independent VTK surface actor
4. Gray value thresholding filters non-brain regions

### Key Technical Implementation
- **Surface rendering** using `vtkMarchingCubes` (NOT volume rendering)
- **PIMPL pattern** for API implementation hiding
- **Per-label processing**: Each label gets own `BrainRegionVolume`
- **Camera-based sorting** to fix VTK 8.2 multi-volume rendering issues

## Current Problem Domain

**Core Issue**: Label NIFTI marks regions outside brain tissue
**Solution**: Filter labels using MRI gray values (user-defined min/max)

### Processing Pipeline
1. Load MRI and label NIFTI files
2. Extract all unique labels from label NIFTI
3. For each label:
   - Create label mask using `vtkImageThreshold`
   - Apply gray value limits to MRI data
   - Multiply filtered MRI with label mask
   - Generate surface mesh with `vtkMarchingCubes`
   - Create independent VTK actor

### Critical Code Locations
- **API entry**: `lib/NiftiVisualizationAPI.cpp`
- **Region processing**: `lib/niftimanager.cpp::processRegions()`
- **Fusion logic**: `lib/brainregionvolume.cpp::setVolumeData()`
- **Gray filtering**: `lib/brainregionvolume.cpp::setGrayValueLimits()`

## Dependencies & Environment

**Required Libraries**:
- VTK 8.2.0 (hardcoded: `D:/code/vtk8.2.0/VTK-8.2.0`)
- Qt 5.12.9 (hardcoded: `C:/Qt/Qt5.12.9/5.12.9/msvc2017_64`)
- CUDA Toolkit (optional, architecture 75)

**Build Tools**:
- Visual Studio 2022 x64
- CMake 3.14+
- Windows platform

## API Design & Usage

### Core API Methods
```cpp
// Setup
setRenderer(vtkRenderer*)

// Data loading
loadMriNifti(QString) → bool
loadLabelNifti(QString) → bool

// Processing
processRegions(minGray, maxGray)
previewMriVisualization(minGray, maxGray)

// Visualization control
setRegionVisibility(label, visible)
setRegionColor(label, color)
setRegionOpacity(label, opacity)
sortVolumesByCamera()

// Callbacks
setErrorCallback(function)
setRegionsProcessedCallback(function)
```

### Integration Rules
1. Example code CANNOT include headers from `lib/`
2. All VTK operations must go through API
3. Thread safety required for all public methods
4. Maintain backward compatibility for API changes

## Known Issues & TODOs

### Current Implementation Issues
- Gray value filtering in `BrainRegionVolume::setVolumeData()` may need optimization
- Ensure complete filtering of non-brain regions with labels
- Performance optimization for large datasets

### Future Improvements
- Dynamic VTK/Qt path configuration
- Multi-resolution alignment refinement
- GPU acceleration integration
- Better error handling for edge cases