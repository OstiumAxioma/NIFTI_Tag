#include "niftimanager.h"
#include "brainregionvolume.h"

#include <QDebug>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>

// VTK头文件
#include <vtkNIFTIImageReader.h>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkRenderer.h>

NiftiManager::NiftiManager(QObject *parent)
    : QObject(parent)
    , mriImage(nullptr)
    , labelImage(nullptr)
    , renderer(nullptr)
{
    qDebug() << "NiftiManager 初始化";
}

NiftiManager::~NiftiManager()
{
    clearRegions();
    qDebug() << "NiftiManager 析构";
}

bool NiftiManager::loadMriNifti(const QString& filePath)
{
    qDebug() << "开始加载MRI NIFTI文件:" << filePath;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit errorOccurred("MRI文件不存在: " + filePath);
        return false;
    }

    try {
        // 使用VTK的NIFTI读取器
        auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
        reader->SetFileName(filePath.toStdString().c_str());
        reader->Update();
        
        mriImage = reader->GetOutput();
        if (!mriImage) {
            emit errorOccurred("无法读取MRI NIFTI文件");
            return false;
        }

        qDebug() << "MRI NIFTI文件加载成功";
        qDebug() << "MRI图像尺寸:" << mriImage->GetDimensions()[0] 
                 << "x" << mriImage->GetDimensions()[1] 
                 << "x" << mriImage->GetDimensions()[2];
        
        return true;
    }
    catch (const std::exception& e) {
        emit errorOccurred("加载MRI文件时发生错误: " + QString(e.what()));
        return false;
    }
}

bool NiftiManager::loadLabelNifti(const QString& filePath)
{
    qDebug() << "开始加载标签NIFTI文件:" << filePath;
    
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        emit errorOccurred("标签文件不存在: " + filePath);
        return false;
    }

    try {
        // 使用VTK的NIFTI读取器
        auto reader = vtkSmartPointer<vtkNIFTIImageReader>::New();
        reader->SetFileName(filePath.toStdString().c_str());
        reader->Update();
        
        labelImage = reader->GetOutput();
        if (!labelImage) {
            emit errorOccurred("无法读取标签NIFTI文件");
            return false;
        }

        qDebug() << "标签NIFTI文件加载成功";
        qDebug() << "标签图像尺寸:" << labelImage->GetDimensions()[0] 
                 << "x" << labelImage->GetDimensions()[1] 
                 << "x" << labelImage->GetDimensions()[2];
        
        return true;
    }
    catch (const std::exception& e) {
        emit errorOccurred("加载标签文件时发生错误: " + QString(e.what()));
        return false;
    }
}

void NiftiManager::processRegions()
{
    if (!mriImage || !labelImage) {
        emit errorOccurred("需要同时加载MRI和标签数据才能处理区块");
        return;
    }

    qDebug() << "开始处理脑区块...";
    
    // 清理旧的区块
    clearRegions();
    
    // 提取所有标签编号
    QList<int> labels = extractLabelsFromImage();
    qDebug() << "发现" << labels.size() << "个标签区块:" << labels;
    
    // 为每个标签创建BrainRegionVolume
    for (int label : labels) {
        if (label == 0) continue; // 跳过背景标签
        
        qDebug() << "正在创建区块" << label;
        
        try {
            auto* regionVolume = new BrainRegionVolume(label, this);
            regionVolume->updateColor(generateColorForLabel(label));
            
            // 设置体数据（MRI数据和标签掩码）
            regionVolume->setVolumeData(mriImage, labelImage);
            
            // 连接信号
            connect(regionVolume, &BrainRegionVolume::visibilityChanged,
                    this, &NiftiManager::regionVisibilityChanged);
            
            regionVolumes[label] = regionVolume;
            
            qDebug() << "区块" << label << "创建成功，颜色:" << regionVolume->getColor().name();
            
            // 延迟添加到渲染器，避免在所有区块创建完成前渲染
            // if (renderer) {
            //     addVolumeToRenderer(regionVolume);
            // }
        }
        catch (const std::exception& e) {
            qDebug() << "创建区块" << label << "时发生错误:" << e.what();
        }
        catch (...) {
            qDebug() << "创建区块" << label << "时发生未知错误";
        }
    }
    
    qDebug() << "脑区块处理完成，共" << regionVolumes.size() << "个区块";
    emit regionsProcessed();
}

void NiftiManager::clearRegions()
{
    // 从渲染器中移除所有Volume
    for (auto* volume : regionVolumes.values()) {
        removeVolumeFromRenderer(volume);
        volume->deleteLater();
    }
    regionVolumes.clear();
}

void NiftiManager::updateRegionVisibility(int label, bool visible)
{
    if (regionVolumes.contains(label)) {
        regionVolumes[label]->updateVisibility(visible);
    }
}

void NiftiManager::sortVolumesByCamera(vtkCamera* camera)
{
    if (!camera || regionVolumes.isEmpty()) return;
    
    // 获取所有可见的区块
    QList<BrainRegionVolume*> visibleVolumes;
    for (auto* volume : regionVolumes.values()) {
        if (volume->isVisible()) {
            visibleVolumes.append(volume);
        }
    }
    
    // 按到相机的距离排序
    std::sort(visibleVolumes.begin(), visibleVolumes.end(),
              [camera](BrainRegionVolume* a, BrainRegionVolume* b) {
                  return a->distanceToCamera(camera) > b->distanceToCamera(camera);
              });
    
    // 重新添加到渲染器（远的先添加）
    if (renderer) {
        for (auto* volume : visibleVolumes) {
            renderer->RemoveVolume(volume->getVolume());
            renderer->AddVolume(volume->getVolume());
        }
    }
}

QList<int> NiftiManager::getAllLabels() const
{
    return regionVolumes.keys();
}

BrainRegionVolume* NiftiManager::getRegionVolume(int label)
{
    return regionVolumes.value(label, nullptr);
}

void NiftiManager::setRenderer(vtkRenderer* renderer)
{
    this->renderer = renderer;
}

QList<int> NiftiManager::extractLabelsFromImage()
{
    QList<int> labels;
    if (!labelImage) return labels;
    
    vtkDataArray* scalars = labelImage->GetPointData()->GetScalars();
    if (!scalars) return labels;
    
    // 遍历所有像素，收集唯一的标签值
    QSet<int> uniqueLabels;
    int numPoints = labelImage->GetNumberOfPoints();
    
    for (int i = 0; i < numPoints; ++i) {
        int label = static_cast<int>(scalars->GetTuple1(i));
        if (label > 0) { // 跳过背景
            uniqueLabels.insert(label);
        }
    }
    
    labels = uniqueLabels.values();
    std::sort(labels.begin(), labels.end());
    return labels;
}

QColor NiftiManager::generateColorForLabel(int label)
{
    // 使用标签值作为种子生成固定但不同的颜色
    QRandomGenerator generator(static_cast<quint32>(label));
    
    // 生成较饱和的颜色
    int hue = generator.bounded(360);
    int saturation = generator.bounded(128, 255);
    int value = generator.bounded(128, 255);
    
    return QColor::fromHsv(hue, saturation, value);
}

void NiftiManager::addVolumeToRenderer(BrainRegionVolume* volume)
{
    if (renderer && volume) {
        renderer->AddVolume(volume->getVolume());
        renderer->AddActor(volume->getCentroidSphere());
    }
}

void NiftiManager::removeVolumeFromRenderer(BrainRegionVolume* volume)
{
    if (renderer && volume) {
        renderer->RemoveVolume(volume->getVolume());
        renderer->RemoveActor(volume->getCentroidSphere());
    }
} 