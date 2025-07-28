#include "brainregionvolume.h"

#include <QDebug>
#include <cmath>
#include <algorithm>

// VTK头文件
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkCamera.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkImageThreshold.h>
#include <vtkImageMathematics.h>
#include <vtkMarchingCubes.h>
#include <vtkImageReslice.h>
#include <vtkAlgorithmOutput.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkImageCast.h>
#include <vtkImageMask.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkLinearSubdivisionFilter.h>

BrainRegionVolume::BrainRegionVolume(int label, QObject *parent)
    : QObject(parent)
    , label(label)
    , color(Qt::red)  // 默认颜色，将在NiftiManager中被覆盖
    , visible(true)
    , centroid(0, 0, 0)
    , minGrayValue(0.0)
    , maxGrayValue(0.0)
    , useGrayValueLimits(false)
    , voxelCount(0)
{
    initializeSurfaceActor();
    qDebug() << "BrainRegionVolume" << label << "初始化，默认颜色:" << color.name();
}

BrainRegionVolume::~BrainRegionVolume()
{
    qDebug() << "BrainRegionVolume" << label << "析构";
}

void BrainRegionVolume::setVolumeData(vtkImageData* labelSpecificMriData, double minGrayValue, double maxGrayValue)
{
    if (!labelSpecificMriData) {
        qDebug() << "警告: 标签专用MRI数据为空";
        return;
    }
    
    // 设置灰度值限制
    this->minGrayValue = minGrayValue;
    this->maxGrayValue = maxGrayValue;
    this->useGrayValueLimits = (minGrayValue < maxGrayValue);

    try {
        qDebug() << "开始处理区块" << label << "的surface数据（新的标签专用方法）";
        
        // 获取数据维度和范围信息
        int* dims = labelSpecificMriData->GetDimensions();
        double* dataRange = labelSpecificMriData->GetScalarRange();
        
        qDebug() << "区块" << label << "数据维度:" << dims[0] << "x" << dims[1] << "x" << dims[2];
        qDebug() << "区块" << label << "数据范围: [" << dataRange[0] << ", " << dataRange[1] << "]";
        
        // 统计非零体素数量
        vtkDataArray* scalars = labelSpecificMriData->GetPointData()->GetScalars();
        voxelCount = 0;
        for (int i = 0; i < scalars->GetNumberOfTuples(); i++) {
            if (scalars->GetComponent(i, 0) > 0.0) {
                voxelCount++;
            }
        }
        qDebug() << "区块" << label << "包含" << voxelCount << "个有效体素";
        
        // 直接使用标签专用的MRI数据，不需要额外的融合处理
        vtkImageData* processedData = labelSpecificMriData;
        
        // 灰度值限制将在Marching Cubes阈值选择时考虑
        if (useGrayValueLimits) {
            qDebug() << "区块" << label << "将在Marching Cubes时考虑灰度值限制: [" 
                     << minGrayValue << ", " << maxGrayValue << "]";
        }
        
        // 生成表面（基于纯粹MRI数据的高质量表面）
        double dataRangeValue = dataRange[1] - dataRange[0];
        
        if (dataRangeValue <= 0 || voxelCount == 0) {
            qDebug() << "区块" << label << "数据无效或无有效体素，跳过处理";
            return;
        }
        qDebug() << "区块" << label << "使用标签专用MRI数据生成高质量表面";
            
        // 对输入数据进行适度的高斯平滑以减少锯齿
        vtkSmartPointer<vtkImageGaussianSmooth> gaussianSmooth;
        vtkImageData* smoothedData = processedData;  // 默认使用原始数据
        
        try {
            qDebug() << "区块" << label << "开始高斯平滑处理";
            gaussianSmooth = vtkSmartPointer<vtkImageGaussianSmooth>::New();
            gaussianSmooth->SetInputData(processedData);
            gaussianSmooth->SetStandardDeviations(0.8, 0.8, 0.8);  // 适中的平滑强度
            gaussianSmooth->SetRadiusFactors(2.0, 2.0, 2.0);      // 适中的平滑半径
            gaussianSmooth->Update();
            smoothedData = gaussianSmooth->GetOutput();
            qDebug() << "区块" << label << "高斯平滑完成";
        } catch (const std::exception& e) {
            qDebug() << "区块" << label << "高斯平滑失败，使用原始数据:" << e.what();
            smoothedData = processedData;
        } catch (...) {
            qDebug() << "区块" << label << "高斯平滑发生未知错误，使用原始数据";
            smoothedData = processedData;
        }
            
            // 创建Marching Cubes
            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(smoothedData);  // 使用安全的数据源
            marchingCubes->ComputeNormalsOn();
            marchingCubes->ComputeGradientsOff();
            
        // 新方法：智能阈值选择（基于纯粹MRI数据）
        double threshold;
        if (useGrayValueLimits) {
            // 使用用户设定的灰度值范围
            threshold = minGrayValue;
            qDebug() << "区块" << label << "使用用户设定阈值:" << threshold;
        } else {
            // 自动选择阈值：使用数据范围的10%作为阈值
            threshold = dataRange[0] + dataRangeValue * 0.1;
            qDebug() << "区块" << label << "使用自动阈值:" << threshold 
                     << "(范围:" << dataRange[0] << "-" << dataRange[1] << ")";
        }
            
        // 设置阈值并生成表面
        marchingCubes->SetValue(0, threshold);
        marchingCubes->SetNumberOfContours(1);
        marchingCubes->Update();
        
        // 检查生成的表面
        vtkPolyData* polyData = marchingCubes->GetOutput();
        if (!polyData || polyData->GetNumberOfPoints() == 0) {
            qDebug() << "区块" << label << "Marching Cubes未生成有效数据，尝试更低阈值";
            
            // 使用非常低的阈值重试
            double minThreshold = dataRange[0] + dataRangeValue * 0.01;
            marchingCubes->SetValue(0, minThreshold);
            marchingCubes->Update();
            
            polyData = marchingCubes->GetOutput();
            if (!polyData || polyData->GetNumberOfPoints() == 0) {
                qDebug() << "区块" << label << "仍无法生成表面";
                return;
            }
        }
            
        qDebug() << "区块" << label << "Marching Cubes生成了" 
                 << polyData->GetNumberOfPoints() << "个点，"
                 << polyData->GetNumberOfCells() << "个面";
        
        // 使用传统平滑器进行表面平滑
        qDebug() << "区块" << label << "使用传统平滑器进行表面优化";
        auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
        smoother->SetInputConnection(marchingCubes->GetOutputPort());
        // 设置传统平滑器参数以获得好的平滑效果
        if (polyData->GetNumberOfPoints() < 10000) {
            // 小模型：适度平滑
            smoother->SetNumberOfIterations(20);
            smoother->SetRelaxationFactor(0.1);
            qDebug() << "区块" << label << "应用适度平滑（小模型）";
        } else if (polyData->GetNumberOfPoints() < 50000) {
            // 中等模型：轻度平滑
            smoother->SetNumberOfIterations(15);
            smoother->SetRelaxationFactor(0.08);
            qDebug() << "区块" << label << "应用轻度平滑（中等模型）";
        } else {
            // 大模型：最小平滑
            smoother->SetNumberOfIterations(10);
            smoother->SetRelaxationFactor(0.05);
            qDebug() << "区块" << label << "应用最小平滑（大模型）";
        }
        
        // 设置传统平滑器其他参数
        smoother->FeatureEdgeSmoothingOn();   // 开启特征边平滑
        smoother->SetFeatureAngle(60.0);      // 设置特征角度
        smoother->BoundarySmoothingOn();      // 平滑边界
        smoother->SetConvergence(0.0);        // 关闭收敛检查
        
        qDebug() << "区块" << label << "执行传统平滑器";
        smoother->Update();
        qDebug() << "区块" << label << "传统平滑器完成";
        // 直接使用传统平滑器的输出作为最终结果
        qDebug() << "区块" << label << "准备最终输出";
        // 简化流程：直接使用平滑器输出，不做额外处理
        // 设置 surfaceMapper
        try {
            if (!surfaceMapper) {
                qDebug() << "区块" << label << "surfaceMapper为空，初始化失败";
                return;
            }
            
            surfaceMapper->SetInputConnection(smoother->GetOutputPort());
            qDebug() << "区块" << label << "surfaceMapper设置完成";
            
        } catch (const std::exception& e) {
            qDebug() << "区块" << label << "设置surfaceMapper失败:" << e.what();
            return;
        } catch (...) {
            qDebug() << "区块" << label << "设置surfaceMapper发生未知错误";
            return;
        }
        // 计算质心
        try {
            calculateCentroid();
            qDebug() << "区块" << label << "质心计算完成";
        } catch (const std::exception& e) {
            qDebug() << "区块" << label << "质心计算失败:" << e.what();
            centroid = QVector3D(0, 0, 0);
        }
        
        qDebug() << "区块" << label << "新方法标签专用MRI surface数据设置完成";
    } catch (const std::exception& e) {
        qDebug() << "设置区块" << label << "体数据时发生错误:" << e.what();
    } catch (...) {
        qDebug() << "设置区块" << label << "体数据时发生未知错误";
    }
}

void BrainRegionVolume::calculateCentroid()
{
    // 从surface mapper获取PolyData而不是ImageData
    if (!surfaceMapper || !surfaceMapper->GetInput()) {
        qDebug() << "区块" << label << "surfaceMapper或输入数据为空";
        centroid = QVector3D(0, 0, 0);
        return;
    }

    vtkPolyData* polyData = surfaceMapper->GetInput();
    if (!polyData || polyData->GetNumberOfPoints() == 0) {
        qDebug() << "区块" << label << "PolyData为空或没有点";
        centroid = QVector3D(0, 0, 0);
        return;
    }

    // 计算PolyData的几何中心
    double bounds[6];
    polyData->GetBounds(bounds);
    
    // bounds: [xmin, xmax, ymin, ymax, zmin, zmax]
    double centerX = (bounds[0] + bounds[1]) / 2.0;
    double centerY = (bounds[2] + bounds[3]) / 2.0;
    double centerZ = (bounds[4] + bounds[5]) / 2.0;
    
    centroid.setX(centerX);
    centroid.setY(centerY);
    centroid.setZ(centerZ);


    qDebug() << "区块" << label << "质心:" << centroid << "（基于PolyData边界）";
}

void BrainRegionVolume::updateVisibility(bool visible)
{
    if (this->visible == visible) return;

    this->visible = visible;
    if (surfaceActor) {
        surfaceActor->SetVisibility(visible);
    }

    qDebug() << "区块" << label << "可见性:" << visible;
    emit visibilityChanged(label, visible);
}

void BrainRegionVolume::updateColor(const QColor& color)
{
    if (this->color == color) return;

    this->color = color;
    updateSurfaceColor();

    qDebug() << "区块" << label << "颜色更新为:" << color.name();
    emit colorChanged(label, color);
}


void BrainRegionVolume::setOpacity(double opacity)
{
    if (surfaceActor) {
        surfaceActor->GetProperty()->SetOpacity(opacity);
    }
}

void BrainRegionVolume::setSampleDistance(double distance)
{
    // Surface渲染不需要采样距离设置
    Q_UNUSED(distance)
    qDebug() << "Surface渲染不支持setSampleDistance";
}

void BrainRegionVolume::setGrayValueLimits(double minGrayValue, double maxGrayValue)
{
    this->minGrayValue = minGrayValue;
    this->maxGrayValue = maxGrayValue;
    this->useGrayValueLimits = (minGrayValue < maxGrayValue);
    
    qDebug() << "区块" << label << "设置灰度值限制: [" << minGrayValue << ", " << maxGrayValue << "]";
}

void BrainRegionVolume::initializeSurfaceActor()
{
    qDebug() << "区块" << label << "开始初始化surface actor";
    
    try {
        // 创建surface映射器
        surfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        
        // 创建surface actor
        surfaceActor = vtkSmartPointer<vtkActor>::New();
        surfaceActor->SetMapper(surfaceMapper);
        
        // 设置基本属性
        setupSurfaceProperty();
        
        qDebug() << "区块" << label << "surface actor初始化完成";
    }
    catch (const std::exception& e) {
        qDebug() << "区块" << label << "初始化失败:" << e.what();
    }
    catch (...) {
        qDebug() << "区块" << label << "初始化失败: 未知错误";
    }
}


void BrainRegionVolume::setupSurfaceProperty()
{
    if (surfaceActor) {
        // 为每个区块创建完全独立的属性对象
        auto property = vtkSmartPointer<vtkProperty>::New();
        
        // 设置基本颜色，使用当前color成员变量
        property->SetColor(color.redF(), color.greenF(), color.blueF());
        
        // 设置光照属性
        property->SetAmbient(0.3);
        property->SetDiffuse(0.7);
        property->SetSpecular(0.2);
        property->SetSpecularPower(10);
        
        // 设置不透明度（完全不透明）
        property->SetOpacity(1.0);
        
        // 强制设置为独立属性（不共享）
        property->SetInterpolationToGouraud();
        
        // 确保使用固定颜色而不是标量颜色
        // VTK Property本身不需要设置标量可见性，这由Mapper控制
        
        // 将属性设置给actor
        surfaceActor->SetProperty(property);
        
        // 强制刷新actor和mapper
        surfaceActor->Modified();
        if (surfaceMapper) {
            surfaceMapper->SetScalarVisibility(false); // 确保mapper不使用标量颜色
            surfaceMapper->Modified();
        }
        
        qDebug() << "区块" << label << "独立属性设置完成，颜色:" << color.name() 
                 << "RGB(" << color.redF() << "," << color.greenF() << "," << color.blueF() << ")";
    }
}

void BrainRegionVolume::updateSurfaceColor()
{
    if (surfaceActor) {
        // 重新创建完全独立的属性对象，确保使用正确的颜色
        auto property = vtkSmartPointer<vtkProperty>::New();
        
        // 设置颜色
        property->SetColor(color.redF(), color.greenF(), color.blueF());
        
        // 设置光照属性
        property->SetAmbient(0.3);
        property->SetDiffuse(0.7);
        property->SetSpecular(0.2);
        property->SetSpecularPower(10);
        
        // 设置不透明度（完全不透明）
        property->SetOpacity(1.0);
        
        // 强制设置为独立属性（不共享）
        property->SetInterpolationToGouraud();
        
        // 将属性设置给actor
        surfaceActor->SetProperty(property);
        
        // 强制刷新actor和mapper
        surfaceActor->Modified();
        if (surfaceMapper) {
            surfaceMapper->SetScalarVisibility(false); // 确保mapper不使用标量颜色
            surfaceMapper->Modified();
        }
        
        qDebug() << "区块" << label << "surface颜色更新为:" << color.name() 
                 << "RGB(" << color.redF() << "," << color.greenF() << "," << color.blueF() << ")";
    } else {
        qDebug() << "区块" << label << "surfaceActor为空，无法设置颜色";
    }
}

void BrainRegionVolume::updateSurfaceOpacity()
{
    if (surfaceActor) {
        surfaceActor->GetProperty()->SetOpacity(visible ? 1.0 : 0.0);
    }
} 