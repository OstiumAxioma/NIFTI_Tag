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
        qDebug() << "开始处理区块" << label << "的surface数据（改进的MRI融合算法）";
        
        // 创建阈值过滤器，提取当前标签的区域
        auto threshold = vtkSmartPointer<vtkImageThreshold>::New();
        threshold->SetInputData(maskData);
        threshold->ThresholdBetween(label, label);
        threshold->SetInValue(1.0);
        threshold->SetOutValue(0.0);
        threshold->Update();

        // 检查阈值结果
        vtkImageData* thresholdOutput = threshold->GetOutput();
        if (!thresholdOutput) {
            qDebug() << "区块" << label << "阈值处理失败";
            return;
        }

        // 获取数据范围
        double* range = thresholdOutput->GetScalarRange();
        if (range[1] <= range[0]) {
            qDebug() << "区块" << label << "数据范围无效: [" << range[0] << ", " << range[1] << "]";
            return;
        }

        // 将标签数据重采样到MRI数据的空间，保持MRI的高精度
        auto resample = vtkSmartPointer<vtkImageReslice>::New();
        resample->SetInputData(thresholdOutput);
        resample->SetOutputDimensionality(3);
        resample->SetOutputSpacing(mriData->GetSpacing());
        resample->SetOutputOrigin(mriData->GetOrigin());
        resample->SetOutputExtent(mriData->GetExtent());
        resample->SetInterpolationModeToNearestNeighbor(); // 保持标签的离散性
        resample->Update();
        
        vtkImageData* resampledMask = resample->GetOutput();
        
        // 检查MRI数据和重采样掩码的标量类型
        int mriScalarType = mriData->GetScalarType();
        int maskScalarType = resampledMask->GetScalarType();
        
        qDebug() << "区块" << label << "MRI标量类型:" << mriScalarType << "掩码标量类型:" << maskScalarType;
        
        // 如果标量类型不匹配，需要转换掩码数据类型以匹配MRI数据
        vtkImageData* finalMask = resampledMask;
        vtkSmartPointer<vtkImageCast> castFilter;
        
        if (mriScalarType != maskScalarType) {
            qDebug() << "区块" << label << "标量类型不匹配，进行类型转换";
            castFilter = vtkSmartPointer<vtkImageCast>::New();
            castFilter->SetInputData(resampledMask);
            castFilter->SetOutputScalarType(mriScalarType);
            castFilter->Update();
            finalMask = castFilter->GetOutput();
            
            qDebug() << "区块" << label << "转换后掩码标量类型:" << finalMask->GetScalarType();
        }
        
        // 获取MRI数据的原始范围
        double* mriRange = mriData->GetScalarRange();
        qDebug() << "区块" << label << "MRI原始数据范围: [" << mriRange[0] << ", " << mriRange[1] << "]";
        
        // 使用ImageMathematics将MRI数据与重采样后的掩码相乘，保留MRI的灰度信息
        auto multiply = vtkSmartPointer<vtkImageMathematics>::New();
        multiply->SetOperationToMultiply();
        multiply->SetInput1Data(mriData);
        multiply->SetInput2Data(finalMask);
        multiply->Update();

        vtkImageData* maskedMriData = multiply->GetOutput();
        double* maskedRange = maskedMriData->GetScalarRange();
        
        qDebug() << "区块" << label << "掩码MRI数据范围: [" << maskedRange[0] << ", " << maskedRange[1] << "]";
        
        // 检查掩码MRI数据是否有效
        if (maskedRange[1] <= maskedRange[0] || maskedRange[1] == 0) {
            qDebug() << "区块" << label << "掩码MRI数据无效，回退到使用标签数据生成surface";
            
            // 回退策略：直接使用阈值化的标签数据
            vtkImageData* fallbackData = thresholdOutput;
            double* fallbackRange = fallbackData->GetScalarRange();
            
            qDebug() << "区块" << label << "使用标签数据范围: [" << fallbackRange[0] << ", " << fallbackRange[1] << "]";
            
            // 使用标签数据生成surface
            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(fallbackData);
            marchingCubes->SetValue(0, 0.5); // 标签数据的阈值
            marchingCubes->ComputeNormalsOn();
            marchingCubes->ComputeGradientsOn();
            marchingCubes->Update();
            
            vtkPolyData* polyData = marchingCubes->GetOutput();
            if (polyData && polyData->GetNumberOfPoints() > 0) {
                qDebug() << "区块" << label << "使用标签数据生成了" << polyData->GetNumberOfPoints() << "个点";
                surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
            } else {
                qDebug() << "区块" << label << "标签数据也无法生成有效surface";
                return;
            }
        } else {
            // 正常情况：使用融合后的MRI数据生成高质量surface
            qDebug() << "区块" << label << "使用融合MRI数据生成高质量surface";
            
            // 应用灰度值限制的改进算法
            vtkImageData* processedData = maskedMriData;
            vtkSmartPointer<vtkImageThreshold> grayThreshold;
            
            if (useGrayValueLimits) {
                qDebug() << "区块" << label << "应用灰度值限制: [" << minGrayValue << ", " << maxGrayValue << "]";
                
                // 创建一个更温和的灰度值过滤器
                grayThreshold = vtkSmartPointer<vtkImageThreshold>::New();
                grayThreshold->SetInputData(maskedMriData);
                grayThreshold->ThresholdBetween(minGrayValue, maxGrayValue);
                grayThreshold->SetInValue(1.0);
                grayThreshold->SetOutValue(0.0);
                grayThreshold->ReplaceInOff();  // 保持原值，不替换
                grayThreshold->ReplaceOutOn();  // 只替换范围外的值
                grayThreshold->Update();
                
                processedData = grayThreshold->GetOutput();
                double* processedRange = processedData->GetScalarRange();
                qDebug() << "区块" << label << "灰度值限制后范围: [" << processedRange[0] << ", " << processedRange[1] << "]";
                
                // 如果灰度值限制后数据太少，回退到原始掩码数据
                if (processedRange[1] <= processedRange[0] || processedRange[1] < minGrayValue * 0.1) {
                    qDebug() << "区块" << label << "灰度值限制过于严格，回退到原始掩码数据";
                    processedData = maskedMriData;
                }
            }
            
            // 获取最终处理数据的范围
            double* finalRange = processedData->GetScalarRange();
            double dataRange = finalRange[1] - finalRange[0];
            
            // 改进的阈值计算 - 防止过度细分
            double dynamicThreshold;
            
            if (useGrayValueLimits && dataRange > 0) {
                // 使用更保守的阈值策略，避免过度细分
                double baseThreshold = finalRange[0] + dataRange * 0.3;  // 提高基础阈值
                
                // 确保阈值不会太低，避免生成过多细小区域
                double minAllowedThreshold = std::max(finalRange[0] + dataRange * 0.2, minGrayValue * 0.8);
                dynamicThreshold = std::max(baseThreshold, minAllowedThreshold);
                
                qDebug() << "区块" << label << "使用保守的灰度值限制阈值:" << dynamicThreshold 
                         << "（数据范围:" << finalRange[0] << "-" << finalRange[1] << "）";
            } else {
                // 传统方法，使用更高的阈值避免过度细分
                if (dataRange > 1000) {
                    dynamicThreshold = finalRange[0] + dataRange * 0.4;  // 提高阈值
                } else if (dataRange > 100) {
                    dynamicThreshold = finalRange[0] + dataRange * 0.25; // 提高阈值
                } else {
                    dynamicThreshold = finalRange[0] + dataRange * 0.15; // 提高阈值
                }
                
                qDebug() << "区块" << label << "使用传统保守阈值:" << dynamicThreshold;
            }
            
            // 使用Marching Cubes生成等值面
            auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
            marchingCubes->SetInputData(processedData);
            marchingCubes->SetValue(0, dynamicThreshold);
            marchingCubes->ComputeNormalsOn();
            marchingCubes->ComputeGradientsOn();
            marchingCubes->Update();

            // 检查Marching Cubes是否生成了有效的多边形数据
            vtkPolyData* polyData = marchingCubes->GetOutput();
            if (!polyData || polyData->GetNumberOfPoints() == 0 || polyData->GetNumberOfCells() == 0) {
                qDebug() << "区块" << label << "Marching Cubes没有生成有效数据，尝试更低阈值";
                
                // 尝试更低的阈值，但不要过低
                double lowerThreshold = finalRange[0] + dataRange * 0.15;
                if (lowerThreshold < dynamicThreshold) {
                    marchingCubes->SetValue(0, lowerThreshold);
                    marchingCubes->Update();
                    
                    polyData = marchingCubes->GetOutput();
                    if (polyData && polyData->GetNumberOfPoints() > 0) {
                        qDebug() << "区块" << label << "使用降低阈值" << lowerThreshold << "生成了" << polyData->GetNumberOfPoints() << "个点";
                    } else {
                        qDebug() << "区块" << label << "降低阈值后仍无法生成有效数据";
                        surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
                        return;
                    }
                } else {
                    qDebug() << "区块" << label << "无法进一步降低阈值，跳过处理";
                    surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
                    return;
                }
            } else {
                qDebug() << "区块" << label << "Marching Cubes成功生成了" << polyData->GetNumberOfPoints() << "个点和" << polyData->GetNumberOfCells() << "个面";
                
                // 检查是否生成了过多的点，这可能导致性能问题
                if (polyData->GetNumberOfPoints() > 100000) {
                    qDebug() << "区块" << label << "警告：生成的点数过多(" << polyData->GetNumberOfPoints() << ")，可能影响性能";
                }
            }
            
            // 添加平滑处理以提高surface质量，但减少迭代次数以提高性能
            auto smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
            smoother->SetInputConnection(marchingCubes->GetOutputPort());
            smoother->SetNumberOfIterations(10);  // 减少迭代次数
            smoother->SetRelaxationFactor(0.1);   // 松弛因子
            smoother->FeatureEdgeSmoothingOff();  // 关闭特征边平滑以保持形状
            smoother->BoundarySmoothingOn();      // 启用边界平滑
            
            try {
                smoother->Update();
                
                // 检查平滑后的数据
                vtkPolyData* smoothedData = smoother->GetOutput();
                if (smoothedData && smoothedData->GetNumberOfPoints() > 0) {
                    qDebug() << "区块" << label << "平滑处理成功，生成" << smoothedData->GetNumberOfPoints() << "个点";
                    surfaceMapper->SetInputConnection(smoother->GetOutputPort());
                } else {
                    qDebug() << "区块" << label << "平滑处理失败，使用原始数据";
                    surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
                }
            } catch (const std::exception& e) {
                qDebug() << "区块" << label << "平滑处理异常:" << e.what() << "，使用原始数据";
                surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
            }
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