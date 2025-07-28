#ifndef MULTICHANNELVOLUMEPROCESSOR_H
#define MULTICHANNELVOLUMEPROCESSOR_H

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include <QObject>
#include <QVector>
#include <QVector3D>
#include "VoxelDataTypes.h"

// 前向声明
class ImprovedNearestNeighborAssigner;

/**
 * @brief 多通道体数据处理器
 * 
 * 处理MRI灰度值和标签数据的融合，创建包含(灰度值, 标签)的数据结构。
 * 对于没有标签的MRI数据点，使用最近邻算法分配标签。
 */
class MultiChannelVolumeProcessor : public QObject
{
    Q_OBJECT

public:
    // 使用共享的VoxelData结构

    explicit MultiChannelVolumeProcessor(QObject *parent = nullptr);
    ~MultiChannelVolumeProcessor();

    /**
     * @brief 设置输入数据
     * @param mriData MRI NIFTI数据
     * @param labelData 标签NIFTI数据
     * @return 是否设置成功
     */
    bool setInputData(vtkImageData* mriData, vtkImageData* labelData);

    /**
     * @brief 处理融合数据
     * 创建包含(灰度值, 标签)的融合数据结构
     * @return 是否处理成功
     */
    bool processFusedData();

    /**
     * @brief 获取融合后的VTK数据
     * @return 包含两个通道的vtkImageData：通道0=灰度值，通道1=标签
     */
    vtkImageData* getFusedVtkData() const { return fusedVtkData; }

    /**
     * @brief 为指定标签创建单独的MRI数据
     * @param label 目标标签
     * @return 只包含该标签区域MRI数据的vtkImageData
     */
    vtkSmartPointer<vtkImageData> createLabelSpecificMriData(int label);

    /**
     * @brief 获取所有唯一标签
     * @return 标签列表
     */
    QVector<int> getUniqueLabels() const { return uniqueLabels; }

    /**
     * @brief 获取数据维度
     */
    void getDimensions(int dims[3]) const;

private:
    // 输入数据
    vtkSmartPointer<vtkImageData> mriData;
    vtkSmartPointer<vtkImageData> labelData;
    
    // 融合后的数据
    vtkSmartPointer<vtkImageData> fusedVtkData;
    QVector<VoxelData> fusedData;  // 内部数据存储
    
    // 数据维度和属性
    int dimensions[3];
    double spacing[3];
    double origin[3];
    
    // 标签信息
    QVector<int> uniqueLabels;
    
    // 改进的最近邻分配器
    ImprovedNearestNeighborAssigner* nearestNeighborAssigner;
    
    /**
     * @brief 验证输入数据的兼容性
     */
    bool validateInputData();
    
    /**
     * @brief 提取唯一标签列表
     */
    void extractUniqueLabels();
    
    /**
     * @brief 为无标签的体素分配最近邻标签（使用改进算法）
     */
    void assignNearestNeighborLabels();
    
    /**
     * @brief 使用传统方法分配最近邻标签（回退方案）
     */
    void assignNearestNeighborLabelsLegacy();
    
    /**
     * @brief 查找最近的有标签体素（传统方法）
     * @param x, y, z 目标体素坐标
     * @return 最近的标签值，如果找不到返回0
     */
    int findNearestLabelLegacy(int x, int y, int z);
    
    /**
     * @brief 创建VTK格式的融合数据
     */
    void createFusedVtkData();
    
    /**
     * @brief 计算3D距离
     */
    float calculateDistance(int x1, int y1, int z1, int x2, int y2, int z2);
    
    /**
     * @brief 获取体素索引
     */
    int getVoxelIndex(int x, int y, int z) const;
    
    /**
     * @brief 检查坐标是否在边界内
     */
    bool isValidCoordinate(int x, int y, int z) const;
};

#endif // MULTICHANNELVOLUMEPROCESSOR_H