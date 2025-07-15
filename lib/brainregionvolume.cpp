#include "brainregionvolume.h"

#include <QDebug>
#include <cmath>

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
#include <vtkImageResize.h>
#include <vtkAlgorithmOutput.h>

BrainRegionVolume::BrainRegionVolume(int label, QObject *parent)
    : QObject(parent)
    , label(label)
    , color(Qt::red)
    , visible(true)
    , centroid(0, 0, 0)
{
    initializeSurfaceActor();
    initializeCentroidSphere();
    qDebug() << "BrainRegionVolume" << label << "初始化";
}

BrainRegionVolume::~BrainRegionVolume()
{
    qDebug() << "BrainRegionVolume" << label << "析构";
}

void BrainRegionVolume::setVolumeData(vtkImageData* mriData, vtkImageData* maskData)
{
    if (!mriData || !maskData) {
        qDebug() << "警告: MRI数据或掩码数据为空";
        return;
    }

    try {
        qDebug() << "开始处理区块" << label << "的surface数据";
        
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

        // 检查数据尺寸是否匹配
        int mriDims[3], maskDims[3];
        mriData->GetDimensions(mriDims);
        thresholdOutput->GetDimensions(maskDims);
        
        qDebug() << "区块" << label << "MRI尺寸:" << mriDims[0] << "x" << mriDims[1] << "x" << mriDims[2];
        qDebug() << "区块" << label << "掩码尺寸:" << maskDims[0] << "x" << maskDims[1] << "x" << maskDims[2];

        vtkImageData* resizedMask = thresholdOutput;
        
        // 如果尺寸不匹配，需要重采样掩码数据
        if (mriDims[0] != maskDims[0] || mriDims[1] != maskDims[1] || mriDims[2] != maskDims[2]) {
            qDebug() << "区块" << label << "尺寸不匹配，进行重采样";
            
            auto resizer = vtkSmartPointer<vtkImageResize>::New();
            resizer->SetInputData(thresholdOutput);
            resizer->SetOutputDimensions(mriDims[0], mriDims[1], mriDims[2]);
            resizer->SetInterpolationModeToNearestNeighbor(); // 使用最近邻插值保持标签值
            resizer->Update();
            
            resizedMask = resizer->GetOutput();
            qDebug() << "区块" << label << "重采样完成";
        }

        // 将掩码与MRI数据相乘，得到该区块的MRI数据
        auto multiply = vtkSmartPointer<vtkImageMathematics>::New();
        multiply->SetOperationToMultiply();
        multiply->SetInput1Data(mriData);
        multiply->SetInput2Data(resizedMask);
        multiply->Update();

        // 检查乘法结果
        vtkImageData* multiplyOutput = multiply->GetOutput();
        if (!multiplyOutput) {
            qDebug() << "区块" << label << "数据融合失败";
            return;
        }

        // 检查输出数据的有效性
        if (multiplyOutput->GetNumberOfPoints() == 0) {
            qDebug() << "区块" << label << "没有有效数据点";
            return;
        }

        // 获取数据范围
        double* range = multiplyOutput->GetScalarRange();
        if (range[1] <= range[0]) {
            qDebug() << "区块" << label << "数据范围无效: [" << range[0] << ", " << range[1] << "]";
            return;
        }

        // 使用Marching Cubes生成等值面
        auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
        marchingCubes->SetInputData(multiplyOutput);
        
        // 设置阈值为数据范围的30%
        double threshold_value = range[0] + (range[1] - range[0]) * 0.3;
        marchingCubes->SetValue(0, threshold_value);
        marchingCubes->Update();
        
        qDebug() << "区块" << label << "数据范围: [" << range[0] << ", " << range[1] << "], 阈值: " << threshold_value;

        // 设置到surface映射器
        surfaceMapper->SetInputConnection(marchingCubes->GetOutputPort());
        
        // 计算质心
        calculateCentroid();
        
        qDebug() << "区块" << label << "surface数据设置完成";

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
    if (!surfaceMapper->GetInput()) return;

    // 从surface mapper获取输入数据
    vtkImageData* imageData = vtkImageData::SafeDownCast(surfaceMapper->GetInputDataObject(0, 0));
    vtkDataArray* scalars = imageData->GetPointData()->GetScalars();
    if (!scalars) return;

    double sum[3] = {0, 0, 0};
    int count = 0;
    int dims[3];
    imageData->GetDimensions(dims);
    double spacing[3];
    imageData->GetSpacing(spacing);
    double origin[3];
    imageData->GetOrigin(origin);

    // 遍历所有非零像素，计算质心
    for (int z = 0; z < dims[2]; ++z) {
        for (int y = 0; y < dims[1]; ++y) {
            for (int x = 0; x < dims[0]; ++x) {
                int index = z * dims[0] * dims[1] + y * dims[0] + x;
                double value = scalars->GetTuple1(index);
                
                if (value > 0) {
                    sum[0] += origin[0] + x * spacing[0];
                    sum[1] += origin[1] + y * spacing[1];
                    sum[2] += origin[2] + z * spacing[2];
                    count++;
                }
            }
        }
    }

    if (count > 0) {
        centroid.setX(sum[0] / count);
        centroid.setY(sum[1] / count);
        centroid.setZ(sum[2] / count);

        // 更新质心球体位置
        auto mapper = vtkPolyDataMapper::SafeDownCast(centroidSphere->GetMapper());
        if (mapper) {
            auto sphereSource = vtkSphereSource::SafeDownCast(mapper->GetInputConnection(0, 0)->GetProducer());
            if (sphereSource) {
                sphereSource->SetCenter(centroid.x(), centroid.y(), centroid.z());
                sphereSource->Modified();
            }
        }

        qDebug() << "区块" << label << "质心:" << centroid;
    }
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
        // 设置基本颜色
        updateSurfaceColor();
        
        // 设置光照属性
        surfaceActor->GetProperty()->SetAmbient(0.3);
        surfaceActor->GetProperty()->SetDiffuse(0.7);
        surfaceActor->GetProperty()->SetSpecular(0.2);
        surfaceActor->GetProperty()->SetSpecularPower(10);
        
        // 设置不透明度
        surfaceActor->GetProperty()->SetOpacity(0.8);
    }
}

void BrainRegionVolume::updateSurfaceColor()
{
    if (surfaceActor) {
        surfaceActor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
    }
}

void BrainRegionVolume::updateSurfaceOpacity()
{
    if (surfaceActor) {
        surfaceActor->GetProperty()->SetOpacity(visible ? 0.8 : 0.0);
    }
} 