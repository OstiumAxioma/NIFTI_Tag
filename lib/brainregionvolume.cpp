#include "brainregionvolume.h"

#include <QDebug>
#include <cmath>

// VTK头文件
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkGPUVolumeRayCastMapper.h>
#include <vtkFixedPointVolumeRayCastMapper.h>
#include <vtkColorTransferFunction.h>
#include <vtkPiecewiseFunction.h>
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
#include <vtkAlgorithmOutput.h>

BrainRegionVolume::BrainRegionVolume(int label, QObject *parent)
    : QObject(parent)
    , label(label)
    , color(Qt::red)
    , visible(true)
    , centroid(0, 0, 0)
{
    initializeVolume();
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
        qDebug() << "开始处理区块" << label << "的体数据";
        
        // 暂时跳过复杂的数据处理，只做基本检查
        qDebug() << "区块" << label << "跳过体数据处理，避免崩溃";
        
        // 简单设置质心为原点
        centroid = QVector3D(0, 0, 0);
        
        qDebug() << "区块" << label << "体数据设置完成（简化版）";
        
        /* 暂时注释掉可能导致崩溃的代码
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

        // 将掩码与MRI数据相乘，得到该区块的MRI数据
        auto multiply = vtkSmartPointer<vtkImageMathematics>::New();
        multiply->SetOperationToMultiply();
        multiply->SetInput1Data(mriData);
        multiply->SetInput2Data(thresholdOutput);
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

        // 设置到体映射器
        volumeMapper->SetInputData(multiplyOutput);
        
        // 延迟更新，避免在数据未准备好时更新
        // volumeMapper->Update();

        // 计算质心
        calculateCentroid();
        */

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
    if (!volumeMapper->GetInput()) return;

    vtkImageData* imageData = volumeMapper->GetInput();
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
    volume->SetVisibility(visible);
    centroidSphere->SetVisibility(false); // 质心球体始终隐藏

    qDebug() << "区块" << label << "可见性:" << visible;
    emit visibilityChanged(label, visible);
}

void BrainRegionVolume::updateColor(const QColor& color)
{
    if (this->color == color) return;

    this->color = color;
    updateVolumeColorTransfer();

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
    if (volumeProperty) {
        auto opacityFunction = volumeProperty->GetScalarOpacity();
        if (opacityFunction) {
            opacityFunction->RemoveAllPoints();
            opacityFunction->AddPoint(0, 0.0);
            opacityFunction->AddPoint(255, opacity);
        }
        volume->Modified();
    }
}

void BrainRegionVolume::setSampleDistance(double distance)
{
    // VTK 8.2中vtkVolumeMapper没有SetSampleDistance方法
    // 使用vtkSmartVolumeMapper的特定方法
    auto smartMapper = vtkSmartVolumeMapper::SafeDownCast(volumeMapper);
    if (smartMapper) {
        smartMapper->SetSampleDistance(distance);
    }
}

void BrainRegionVolume::initializeVolume()
{
    qDebug() << "区块" << label << "开始初始化（暂时禁用体绘制）";
    
    // 完全跳过体绘制初始化，避免VTK RayCast崩溃
    try {
        // 创建空的体对象，不设置映射器
        volume = vtkSmartPointer<vtkVolume>::New();
        volume->SetVisibility(false);
        
        qDebug() << "区块" << label << "基础对象初始化完成（无体绘制）";
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

void BrainRegionVolume::setupVolumeProperty()
{
    // 设置颜色传递函数
    auto colorFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
    updateVolumeColorTransfer();
    volumeProperty->SetColor(colorFunction);

    // 设置不透明度传递函数
    auto opacityFunction = vtkSmartPointer<vtkPiecewiseFunction>::New();
    opacityFunction->AddPoint(0, 0.0);
    opacityFunction->AddPoint(255, 0.8);
    volumeProperty->SetScalarOpacity(opacityFunction);

    // 设置渲染模式
    volumeProperty->SetInterpolationTypeToLinear();
    volumeProperty->ShadeOn();
    volumeProperty->SetAmbient(0.4);
    volumeProperty->SetDiffuse(0.6);
    volumeProperty->SetSpecular(0.2);
}

void BrainRegionVolume::updateVolumeColorTransfer()
{
    auto colorFunction = volumeProperty->GetRGBTransferFunction();
    if (!colorFunction) {
        colorFunction = vtkSmartPointer<vtkColorTransferFunction>::New();
        volumeProperty->SetColor(colorFunction);
    }

    colorFunction->RemoveAllPoints();
    
    // 背景透明
    colorFunction->AddRGBPoint(0, 0, 0, 0);
    
    // 使用标签颜色，但根据灰度值调整亮度
    double r = color.redF();
    double g = color.greenF();
    double b = color.blueF();
    
    // 创建从暗到亮的颜色渐变
    colorFunction->AddRGBPoint(1, r*0.2, g*0.2, b*0.2);     // 暗
    colorFunction->AddRGBPoint(128, r*0.6, g*0.6, b*0.6);   // 中等
    colorFunction->AddRGBPoint(255, r, g, b);               // 亮
}

void BrainRegionVolume::updateVolumeOpacity()
{
    auto opacityFunction = volumeProperty->GetScalarOpacity();
    if (opacityFunction) {
        opacityFunction->RemoveAllPoints();
        opacityFunction->AddPoint(0, 0.0);
        opacityFunction->AddPoint(255, visible ? 0.8 : 0.0);
    }
} 