#include "brainregionvolume.h"

#include <QDebug>
#include <cmath>
#include <algorithm>

// VTK头文件
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkCamera.h>
#include <vtkSphereSource.h>
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

BrainRegionVolume::BrainRegionVolume(int label, QObject *parent)
    : QObject(parent)
    , label(label)
    , color(Qt::red)  // 默认颜色，将在NiftiManager中被覆盖
    , visible(true)
    , centroid(0, 0, 0)
    , minGrayValue(0.0)
    , maxGrayValue(0.0)
    , useGrayValueLimits(false)
{
    initializeSurfaceActor();
    initializeCentroidSphere();
    qDebug() << "BrainRegionVolume" << label << "初始化，默认颜色:" << color.name();
}

BrainRegionVolume::~BrainRegionVolume()
{
    qDebug() << "BrainRegionVolume" << label << "析构";
}

void BrainRegionVolume::setVolumeData(vtkImageData* mriData, vtkImageData* maskData)
{
    setVolumeData(mriData, maskData, 0.0, 0.0);
}

void BrainRegionVolume::setVolumeData(vtkImageData* mriData, vtkImageData* maskData, double minGrayValue, double maxGrayValue)
{
    if (!mriData || !maskData) {
        qDebug() << "警告: MRI数据或掩码数据为空";
        return;
    }
    
    // 设置灰度值限制
    this->minGrayValue = minGrayValue;
    this->maxGrayValue = maxGrayValue;
    this->useGrayValueLimits = (minGrayValue < maxGrayValue);

    try {
        qDebug() << "开始处理区块" << label << "的surface数据（新的填充算法）";
        
        // 步骤1: 创建二值化的标签掩码
        auto labelThreshold = vtkSmartPointer<vtkImageThreshold>::New();
        labelThreshold->SetInputData(maskData);
        labelThreshold->ThresholdBetween(label, label);
        labelThreshold->SetInValue(1.0);
        labelThreshold->SetOutValue(0.0);
        labelThreshold->ReplaceInOn();
        labelThreshold->ReplaceOutOn();
        labelThreshold->Update();

        // 检查标签掩码
        vtkImageData* labelMask = labelThreshold->GetOutput();
        if (!labelMask) {
            qDebug() << "区块" << label << "标签掩码创建失败";
            return;
        }

        // 获取数据维度和类型信息
        int* dims = mriData->GetDimensions();
        qDebug() << "区块" << label << "数据维度:" << dims[0] << "x" << dims[1] << "x" << dims[2];
        qDebug() << "区块" << label << "MRI数据类型:" << mriData->GetScalarType() 
                 << "标签掩码类型:" << labelMask->GetScalarType();

        // 步骤2: 确保数据类型匹配后再进行乘法操作
        // 将标签掩码转换为与MRI数据相同的类型
        auto castFilter = vtkSmartPointer<vtkImageCast>::New();
        castFilter->SetInputData(labelMask);
        castFilter->SetOutputScalarType(mriData->GetScalarType());
        castFilter->Update();
        
        vtkImageData* castedMask = castFilter->GetOutput();
        
        // 现在可以安全地进行乘法操作
        auto maskedRegion = vtkSmartPointer<vtkImageMathematics>::New();
        maskedRegion->SetOperationToMultiply();
        maskedRegion->SetInput1Data(mriData);
        maskedRegion->SetInput2Data(castedMask);
        maskedRegion->Update();
        
        vtkImageData* regionData = maskedRegion->GetOutput();
        
        // 获取掩码后的数据范围
        double* regionRange = regionData->GetScalarRange();
        qDebug() << "区块" << label << "标签区域内MRI数据范围: [" << regionRange[0] << ", " << regionRange[1] << "]";
        
        // 步骤3: 直接使用标签区域内的MRI数据，不额外过滤
        vtkImageData* processedData = regionData;
        
        // 记录是否使用了灰度值限制，但不在这里应用
        // 灰度值限制将在Marching Cubes阈值选择时考虑
        if (useGrayValueLimits) {
            qDebug() << "区块" << label << "将在Marching Cubes时考虑灰度值限制: [" 
                     << minGrayValue << ", " << maxGrayValue << "]";
        }
        
        // 步骤4: 生成表面（使用改进的多级阈值策略）
        double* finalRange = processedData->GetScalarRange();
        double dataRange = finalRange[1] - finalRange[0];
        
        if (dataRange <= 0) {
            qDebug() << "区块" << label << "处理后数据无效范围，尝试使用标签掩码";
            
            // 回退策略：使用标签掩码生成简单表面
            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(labelMask);
            marchingCubes->SetValue(0, 0.5);
            marchingCubes->ComputeNormalsOn();
            marchingCubes->Update();
            
            vtkPolyData* polyData = marchingCubes->GetOutput();
            if (polyData && polyData->GetNumberOfPoints() > 0) {
                qDebug() << "区块" << label << "使用标签掩码生成了" << polyData->GetNumberOfPoints() << "个点";
                surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
            } else {
                qDebug() << "区块" << label << "无法生成有效表面";
                return;
            }
        } else {
            qDebug() << "区块" << label << "使用MRI数据生成详细表面";
            
            // 创建Marching Cubes
            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(processedData);
            marchingCubes->ComputeNormalsOn();
            marchingCubes->ComputeGradientsOff();
            
            // 使用非常低的阈值来确保生成完整表面
            double threshold;
            
            if (useGrayValueLimits) {
                // 使用最小灰度值作为基准，稍微降低以确保包含所有有效数据
                threshold = minGrayValue * 0.5;  // 使用50%的最小灰度值
                if (threshold < finalRange[0] + 1.0) {
                    threshold = finalRange[0] + 1.0;  // 确保阈值略高于最小值
                }
                qDebug() << "区块" << label << "使用灰度值阈值:" << threshold 
                         << "(minGray=" << minGrayValue << ")";
            } else {
                // 不使用灰度值限制时，使用非常低的阈值
                // 只略高于背景值，以包含所有非零数据
                threshold = finalRange[0] + dataRange * 0.01;  // 只使用1%的范围
                if (threshold <= finalRange[0]) {
                    threshold = finalRange[0] + 1.0;
                }
                qDebug() << "区块" << label << "使用低阈值:" << threshold 
                         << "(范围:" << finalRange[0] << "-" << finalRange[1] << ")";
            }
            
            // 设置单一阈值
            marchingCubes->SetValue(0, threshold);
            marchingCubes->SetNumberOfContours(1);
            
            marchingCubes->Update();
            
            // 检查生成的表面
            vtkPolyData* polyData = marchingCubes->GetOutput();
            if (!polyData || polyData->GetNumberOfPoints() == 0) {
                qDebug() << "区块" << label << "Marching Cubes未生成有效数据，尝试更低阈值";
                
                // 使用非常低的阈值重试
                double minThreshold = finalRange[0] + (finalRange[1] - finalRange[0]) * 0.01;
                marchingCubes->SetValue(0, minThreshold);
                marchingCubes->SetNumberOfContours(1);
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
            
            // 应用平滑处理来填充小孔并改善表面质量
            auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
            smoother->SetInputConnection(marchingCubes->GetOutputPort());
            
            // 根据点数调整平滑参数
            if (polyData->GetNumberOfPoints() < 10000) {
                // 小模型：更多迭代，更强平滑
                smoother->SetNumberOfIterations(50);
                smoother->SetRelaxationFactor(0.15);
                qDebug() << "区块" << label << "应用强平滑（小模型）";
            } else if (polyData->GetNumberOfPoints() < 50000) {
                // 中等模型：适度平滑
                smoother->SetNumberOfIterations(30);
                smoother->SetRelaxationFactor(0.1);
                qDebug() << "区块" << label << "应用中等平滑";
            } else {
                // 大模型：轻微平滑以保持性能
                smoother->SetNumberOfIterations(15);
                smoother->SetRelaxationFactor(0.05);
                qDebug() << "区块" << label << "应用轻微平滑（大模型）";
            }
            
            smoother->FeatureEdgeSmoothingOff();  // 关闭特征边平滑，让表面更连续
            smoother->BoundarySmoothingOn();      // 平滑边界
            smoother->Update();
            
            surfaceMapper->SetInputConnection(smoother->GetOutputPort());
        }
        
        // 计算质心（安全检查）
        try {
            calculateCentroid();
            qDebug() << "区块" << label << "质心计算完成";
        } catch (const std::exception& e) {
            qDebug() << "区块" << label << "质心计算失败:" << e.what();
            centroid = QVector3D(0, 0, 0); // 设置默认质心
        }
        
        qDebug() << "区块" << label << "改进的MRI融合surface数据设置完成";

    }
    catch (const std::exception& e) {
        qDebug() << "设置区块" << label << "体数据时发生错误:" << e.what();
    }
    catch (...) {
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

    // 更新质心球体位置
    if (centroidSphere) {
        auto mapper = vtkPolyDataMapper::SafeDownCast(centroidSphere->GetMapper());
        if (mapper) {
            auto sphereSource = vtkSphereSource::SafeDownCast(mapper->GetInputConnection(0, 0)->GetProducer());
            if (sphereSource) {
                sphereSource->SetCenter(centroid.x(), centroid.y(), centroid.z());
                sphereSource->Modified();
            }
        }
    }

    qDebug() << "区块" << label << "质心:" << centroid << "（基于PolyData边界）";
}

void BrainRegionVolume::updateVisibility(bool visible)
{
    if (this->visible == visible) return;

    this->visible = visible;
    if (surfaceActor) {
        surfaceActor->SetVisibility(visible);
    }
    centroidSphere->SetVisibility(false); // 质心球体始终隐藏

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

double BrainRegionVolume::distanceToCamera(vtkCamera* camera) const
{
    if (!camera) return 0.0;

    double* cameraPos = camera->GetPosition();
    double dx = centroid.x() - cameraPos[0];
    double dy = centroid.y() - cameraPos[1];
    double dz = centroid.z() - cameraPos[2];

    return std::sqrt(dx*dx + dy*dy + dz*dz);
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

void BrainRegionVolume::initializeCentroidSphere()
{
    // 创建球体源
    auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetRadius(2.0);
    sphereSource->SetPhiResolution(8);
    sphereSource->SetThetaResolution(8);

    // 创建映射器
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(sphereSource->GetOutputPort());

    // 创建演员
    centroidSphere = vtkSmartPointer<vtkActor>::New();
    centroidSphere->SetMapper(mapper);
    
    // 设置极低透明度（几乎不可见）
    centroidSphere->GetProperty()->SetOpacity(0.001);
    centroidSphere->SetVisibility(false);
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