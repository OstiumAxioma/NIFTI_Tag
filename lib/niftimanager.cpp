#include "niftimanager.h"
#include "brainregionvolume.h"
#include "MultiChannelVolumeProcessor.h"

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
    , volumeProcessor(nullptr)
{
    qDebug() << "NiftiManager 初始化";
}

NiftiManager::~NiftiManager()
{
    clearRegions();
    if (volumeProcessor) {
        delete volumeProcessor;
        volumeProcessor = nullptr;
    }
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
    processRegions(0.0, 0.0);
}

void NiftiManager::processRegions(double minGrayValue, double maxGrayValue)
{
    if (!mriImage || !labelImage) {
        emit errorOccurred("需要同时加载MRI和标签数据才能处理区块");
        return;
    }

    qDebug() << "开始处理脑区块（使用新的标签专用方法）...";
    
    // 清理旧的区块
    clearRegions();
    
    // 使用新的处理方法
    processRegionsWithNewMethod(minGrayValue, maxGrayValue);
    
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
    QRandomGenerator generator(static_cast<quint32>(label * 12345)); // 使用乘数增加随机性
    
    // 预定义一些鲜艳的颜色作为基础
    QList<QColor> baseColors = {
        QColor(255, 0, 0),     // 红色
        QColor(0, 255, 0),     // 绿色
        QColor(0, 0, 255),     // 蓝色
        QColor(255, 255, 0),   // 黄色
        QColor(255, 0, 255),   // 洋红
        QColor(0, 255, 255),   // 青色
        QColor(255, 128, 0),   // 橙色
        QColor(128, 0, 255),   // 紫色
        QColor(255, 0, 128),   // 粉红
        QColor(128, 255, 0),   // 青绿
        QColor(0, 128, 255),   // 天蓝
        QColor(255, 128, 128), // 浅红
    };
    
    // 如果标签数量少，直接使用预定义颜色
    if (label > 0 && label <= baseColors.size()) {
        return baseColors[label - 1];
    }
    
    // 否则生成随机但饱和的颜色
    int hue = generator.bounded(360);
    int saturation = generator.bounded(180, 255);  // 更高的饱和度
    int value = generator.bounded(150, 255);       // 更高的亮度
    
    QColor generatedColor = QColor::fromHsv(hue, saturation, value);
    
    qDebug() << "为标签" << label << "生成颜色: HSV(" << hue << "," << saturation << "," << value << ") = " << generatedColor.name();
    
    return generatedColor;
}

void NiftiManager::addVolumeToRenderer(BrainRegionVolume* volume)
{
    if (renderer && volume) {
        renderer->AddActor(volume->getSurfaceActor());
    }
}

void NiftiManager::removeVolumeFromRenderer(BrainRegionVolume* volume)
{
    if (renderer && volume) {
        renderer->RemoveActor(volume->getSurfaceActor());
    }
}

void NiftiManager::setGrayValueLimits(double minGrayValue, double maxGrayValue)
{
    qDebug() << "为所有区块设置灰度值限制: [" << minGrayValue << ", " << maxGrayValue << "]";
    
    for (auto* volume : regionVolumes.values()) {
        if (volume) {
            volume->setGrayValueLimits(minGrayValue, maxGrayValue);
        }
    }
}

bool NiftiManager::initializeVolumeProcessor()
{
    if (volumeProcessor) {
        delete volumeProcessor;
    }
    
    volumeProcessor = new MultiChannelVolumeProcessor(this);
    
    if (!volumeProcessor->setInputData(mriImage, labelImage)) {
        qDebug() << "错误：无法设置多通道处理器输入数据";
        delete volumeProcessor;
        volumeProcessor = nullptr;
        return false;
    }
    
    if (!volumeProcessor->processFusedData()) {
        qDebug() << "错误：无法处理融合数据";
        delete volumeProcessor;
        volumeProcessor = nullptr;
        return false;
    }
    
    qDebug() << "多通道处理器初始化成功";
    return true;
}

void NiftiManager::processRegionsWithNewMethod(double minGrayValue, double maxGrayValue)
{
    // 初始化多通道处理器
    if (!initializeVolumeProcessor()) {
        emit errorOccurred("无法初始化多通道处理器");
        return;
    }
    
    // 获取所有唯一标签
    QVector<int> labels = volumeProcessor->getUniqueLabels();
    qDebug() << "发现" << labels.size() << "个标签区块:" << labels;
    
    // 为每个标签创建BrainRegionVolume
    for (int label : labels) {
        if (label == 0) continue; // 跳过背景标签
        
        qDebug() << "正在创建区块" << label << "（新方法）";
        
        try {
            auto* regionVolume = new BrainRegionVolume(label, this);
            
            // 为每个区块生成独特的颜色
            QColor uniqueColor = generateColorForLabel(label);
            regionVolume->updateColor(uniqueColor);
            
            qDebug() << "区块" << label << "分配颜色:" << uniqueColor.name();
            
            // 使用新方法：创建标签专用的MRI数据
            vtkSmartPointer<vtkImageData> labelSpecificData = 
                volumeProcessor->createLabelSpecificMriData(label);
            
            if (labelSpecificData) {
                // 使用新的setVolumeData方法
                regionVolume->setVolumeData(labelSpecificData, minGrayValue, maxGrayValue);
                qDebug() << "区块" << label << "使用新方法设置数据成功";
            } else {
                qDebug() << "错误：无法为标签" << label << "创建专用MRI数据";
                delete regionVolume;
                continue;
            }
            
            // 连接信号
            connect(regionVolume, &BrainRegionVolume::visibilityChanged,
                    this, &NiftiManager::regionVisibilityChanged);
            
            regionVolumes[label] = regionVolume;
            
            qDebug() << "区块" << label << "创建成功，包含" 
                     << regionVolume->getVoxelCount() << "个有效体素";
            
            // 添加到渲染器
            if (renderer) {
                addVolumeToRenderer(regionVolume);
            }
        }
        catch (const std::exception& e) {
            qDebug() << "创建区块" << label << "时发生错误:" << e.what();
        }
        catch (...) {
            qDebug() << "创建区块" << label << "时发生未知错误";
        }
    }
} 