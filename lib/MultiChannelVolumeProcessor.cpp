#include "MultiChannelVolumeProcessor.h"
#include "ImprovedNearestNeighborAssigner.h"
#include <QDebug>
#include <QSet>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkDataArray.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkImageThreshold.h>
#include <algorithm>
#include <queue>
#include <cmath>

MultiChannelVolumeProcessor::MultiChannelVolumeProcessor(QObject *parent)
    : QObject(parent)
    , mriData(nullptr)
    , labelData(nullptr)
    , fusedVtkData(nullptr)
    , nearestNeighborAssigner(nullptr)
{
    dimensions[0] = dimensions[1] = dimensions[2] = 0;
    spacing[0] = spacing[1] = spacing[2] = 1.0;
    origin[0] = origin[1] = origin[2] = 0.0;
}

MultiChannelVolumeProcessor::~MultiChannelVolumeProcessor()
{
    if (nearestNeighborAssigner) {
        delete nearestNeighborAssigner;
        nearestNeighborAssigner = nullptr;
    }
    qDebug() << "MultiChannelVolumeProcessor析构";
}

bool MultiChannelVolumeProcessor::setInputData(vtkImageData* mriData, vtkImageData* labelData)
{
    if (!mriData || !labelData) {
        qDebug() << "错误：输入数据为空";
        return false;
    }
    
    this->mriData = mriData;
    this->labelData = labelData;
    
    if (!validateInputData()) {
        qDebug() << "错误：输入数据验证失败";
        return false;
    }
    
    // 获取数据属性
    mriData->GetDimensions(dimensions);
    mriData->GetSpacing(spacing);
    mriData->GetOrigin(origin);
    
    qDebug() << "多通道处理器：输入数据设置成功";
    qDebug() << "数据维度：" << dimensions[0] << "x" << dimensions[1] << "x" << dimensions[2];
    
    return true;
}

bool MultiChannelVolumeProcessor::validateInputData()
{
    if (!mriData || !labelData) {
        return false;
    }
    
    // 检查维度是否匹配
    int mriDims[3], labelDims[3];
    mriData->GetDimensions(mriDims);
    labelData->GetDimensions(labelDims);
    
    for (int i = 0; i < 3; i++) {
        if (mriDims[i] != labelDims[i]) {
            qDebug() << "错误：MRI和标签数据维度不匹配";
            qDebug() << "MRI维度：" << mriDims[0] << "x" << mriDims[1] << "x" << mriDims[2];
            qDebug() << "标签维度：" << labelDims[0] << "x" << labelDims[1] << "x" << labelDims[2];
            return false;
        }
    }
    
    return true;
}

bool MultiChannelVolumeProcessor::processFusedData()
{
    if (!mriData || !labelData) {
        qDebug() << "错误：输入数据未设置";
        return false;
    }
    
    qDebug() << "开始处理融合数据...";
    
    // 获取数据数组
    vtkDataArray* mriArray = mriData->GetPointData()->GetScalars();
    vtkDataArray* labelArray = labelData->GetPointData()->GetScalars();
    
    if (!mriArray || !labelArray) {
        qDebug() << "错误：无法获取数据数组";
        return false;
    }
    
    int totalVoxels = dimensions[0] * dimensions[1] * dimensions[2];
    fusedData.clear();
    fusedData.reserve(totalVoxels);
    
    qDebug() << "处理" << totalVoxels << "个体素...";
    
    // 第一步：创建初始融合数据
    for (int i = 0; i < totalVoxels; i++) {
        float grayValue = mriArray->GetComponent(i, 0);
        int label = static_cast<int>(labelArray->GetComponent(i, 0));
        
        fusedData.append(VoxelData(grayValue, label));
    }
    
    // 第二步：提取唯一标签
    extractUniqueLabels();
    
    // 第三步：为无标签体素分配最近邻标签
    assignNearestNeighborLabels();
    
    // 第四步：创建VTK格式的融合数据
    createFusedVtkData();
    
    qDebug() << "融合数据处理完成！";
    qDebug() << "发现" << uniqueLabels.size() << "个唯一标签：" << uniqueLabels;
    
    return true;
}

void MultiChannelVolumeProcessor::extractUniqueLabels()
{
    uniqueLabels.clear();
    QSet<int> labelSet;
    
    for (const VoxelData& voxel : fusedData) {
        if (voxel.label > 0) {  // 忽略背景标签0
            labelSet.insert(voxel.label);
        }
    }
    
    QList<int> labelList = labelSet.values();
    uniqueLabels = QVector<int>::fromList(labelList);
    std::sort(uniqueLabels.begin(), uniqueLabels.end());
    
    qDebug() << "提取到" << uniqueLabels.size() << "个唯一标签";
}

void MultiChannelVolumeProcessor::assignNearestNeighborLabels()
{
    qDebug() << "使用改进算法分配最近邻标签...";
    
    // 初始化改进的分配器
    if (nearestNeighborAssigner) {
        delete nearestNeighborAssigner;
    }
    nearestNeighborAssigner = new ImprovedNearestNeighborAssigner(dimensions, spacing);
    
    // 使用改进算法分配标签
    int assignedCount = nearestNeighborAssigner->assignNearestNeighborLabels(fusedData, uniqueLabels);
    
    qDebug() << "改进算法分配完成，共分配了" << assignedCount << "个体素";
}

void MultiChannelVolumeProcessor::assignNearestNeighborLabelsLegacy()
{
    qDebug() << "使用传统方法分配最近邻标签(回退方案)...";
    
    int assignedCount = 0;
    int totalVoxels = fusedData.size();
    
    for (int i = 0; i < totalVoxels; i++) {
        VoxelData& voxel = fusedData[i];
        
        // 只处理无标签的体素（标签为0且MRI值不为0）
        if (voxel.label == 0 && voxel.grayValue > 0.0f) {
            // 将线性索引转换为3D坐标
            int z = i / (dimensions[0] * dimensions[1]);
            int y = (i % (dimensions[0] * dimensions[1])) / dimensions[0];
            int x = i % dimensions[0];
            
            int nearestLabel = findNearestLabelLegacy(x, y, z);
            if (nearestLabel > 0) {
                voxel.label = nearestLabel;
                assignedCount++;
            }
        }
    }
    
    qDebug() << "传统方法分配完成，共分配了" << assignedCount << "个体素";
}

int MultiChannelVolumeProcessor::findNearestLabelLegacy(int x, int y, int z)
{
    const int maxSearchRadius = 25;  // 扩大搜索半径
    
    for (int radius = 1; radius <= maxSearchRadius; radius++) {
        // 在当前半径内搜索所有点（不只是边界）
        for (int dz = -radius; dz <= radius; dz++) {
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    // 跳过中心点
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }
                    
                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;
                    
                    if (isValidCoordinate(nx, ny, nz)) {
                        int index = getVoxelIndex(nx, ny, nz);
                        if (index >= 0 && index < fusedData.size()) {
                            int label = fusedData[index].label;
                            if (label > 0) {
                                return label;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // 如果在最大搜索半径内找不到标签，返回0（保持无标签状态）
    return 0;
}

void MultiChannelVolumeProcessor::createFusedVtkData()
{
    qDebug() << "创建VTK格式的融合数据...";
    
    fusedVtkData = vtkSmartPointer<vtkImageData>::New();
    fusedVtkData->SetDimensions(dimensions);
    fusedVtkData->SetSpacing(spacing);
    fusedVtkData->SetOrigin(origin);
    
    // 创建两个通道：通道0=灰度值，通道1=标签
    fusedVtkData->AllocateScalars(VTK_FLOAT, 2);
    
    vtkDataArray* scalars = fusedVtkData->GetPointData()->GetScalars();
    
    for (int i = 0; i < fusedData.size(); i++) {
        const VoxelData& voxel = fusedData[i];
        scalars->SetComponent(i, 0, voxel.grayValue);  // 通道0：灰度值
        scalars->SetComponent(i, 1, static_cast<float>(voxel.label));  // 通道1：标签
    }
    
    fusedVtkData->Modified();
    qDebug() << "VTK融合数据创建完成";
}

vtkSmartPointer<vtkImageData> MultiChannelVolumeProcessor::createLabelSpecificMriData(int label)
{
    if (!fusedVtkData) {
        qDebug() << "错误：融合数据未处理";
        return nullptr;
    }
    
    qDebug() << "为标签" << label << "创建专用MRI数据...";
    
    auto labelSpecificData = vtkSmartPointer<vtkImageData>::New();
    labelSpecificData->SetDimensions(dimensions);
    labelSpecificData->SetSpacing(spacing);
    labelSpecificData->SetOrigin(origin);
    labelSpecificData->AllocateScalars(VTK_FLOAT, 1);
    
    vtkDataArray* inputScalars = fusedVtkData->GetPointData()->GetScalars();
    vtkDataArray* outputScalars = labelSpecificData->GetPointData()->GetScalars();
    
    int validVoxelCount = 0;
    int totalVoxels = fusedData.size();
    
    for (int i = 0; i < totalVoxels; i++) {
        float grayValue = inputScalars->GetComponent(i, 0);
        int voxelLabel = static_cast<int>(inputScalars->GetComponent(i, 1));
        
        if (voxelLabel == label) {
            outputScalars->SetComponent(i, 0, grayValue);
            validVoxelCount++;
        } else {
            outputScalars->SetComponent(i, 0, 0.0f);  // 背景值
        }
    }
    
    labelSpecificData->Modified();
    
    qDebug() << "标签" << label << "专用数据创建完成，包含" << validVoxelCount << "个有效体素";
    
    return labelSpecificData;
}

void MultiChannelVolumeProcessor::getDimensions(int dims[3]) const
{
    dims[0] = dimensions[0];
    dims[1] = dimensions[1];
    dims[2] = dimensions[2];
}

float MultiChannelVolumeProcessor::calculateDistance(int x1, int y1, int z1, int x2, int y2, int z2)
{
    float dx = (x1 - x2) * spacing[0];
    float dy = (y1 - y2) * spacing[1];
    float dz = (z1 - z2) * spacing[2];
    return sqrt(dx*dx + dy*dy + dz*dz);
}

int MultiChannelVolumeProcessor::getVoxelIndex(int x, int y, int z) const
{
    return z * dimensions[0] * dimensions[1] + y * dimensions[0] + x;
}

bool MultiChannelVolumeProcessor::isValidCoordinate(int x, int y, int z) const
{
    return x >= 0 && x < dimensions[0] &&
           y >= 0 && y < dimensions[1] &&
           z >= 0 && z < dimensions[2];
}