#ifndef IMPROVEDNEARESTNEIGHBORASSIGNER_H
#define IMPROVEDNEARESTNEIGHBORASSIGNER_H

#include <QVector>
#include <QVector3D>
#include <QPair>
#include <QDebug>
#include <cmath>
#include <queue>
#include "VoxelDataTypes.h"

/**
 * @brief 改进的最近邻标签分配器
 * 
 * 解决原始算法的问题：
 * 1. 使用基于距离的优先队列搜索
 * 2. 考虑MRI灰度值相似性
 * 3. 增大搜索半径
 * 4. 多重验证机制
 */
class ImprovedNearestNeighborAssigner
{
public:
    // 使用共享的VoxelData结构
    
    struct SearchCandidate {
        int x, y, z;
        int label;
        float distance;
        float grayValue;
        
        // 优先队列排序：距离近的优先，灰度值相似的优先
        bool operator<(const SearchCandidate& other) const {
            return distance > other.distance; // 小顶堆
        }
    };

public:
    ImprovedNearestNeighborAssigner(int* dims, double* spacing);
    
    /**
     * @brief 改进的最近邻标签分配
     * @param fusedData 融合数据引用
     * @param uniqueLabels 所有可用标签
     * @return 分配的体素数量
     */
    int assignNearestNeighborLabels(QVector<VoxelData>& fusedData, 
                                   const QVector<int>& uniqueLabels);

private:
    int dimensions[3];
    double spacing[3];
    
    /**
     * @brief 智能最近邻搜索
     * @param x, y, z 目标体素坐标
     * @param targetGray 目标灰度值
     * @param fusedData 数据源
     * @return 最佳匹配的标签
     */
    int findBestMatchingLabel(int x, int y, int z, float targetGray,
                             const QVector<VoxelData>& fusedData);
    
    /**
     * @brief 基于优先队列的搜索
     */
    int priorityQueueSearch(int x, int y, int z, float targetGray,
                           const QVector<VoxelData>& fusedData);
    
    /**
     * @brief 计算两个灰度值的相似性权重
     */
    float calculateGraySimilarity(float gray1, float gray2);
    
    /**
     * @brief 计算综合匹配分数
     */
    float calculateMatchScore(float distance, float graySimilarity);
    
    /**
     * @brief 区域连通性检查
     */
    bool isInSameRegion(int x1, int y1, int z1, int x2, int y2, int z2,
                       const QVector<VoxelData>& fusedData);
    
    /**
     * @brief 工具函数
     */
    float calculateDistance(int x1, int y1, int z1, int x2, int y2, int z2);
    int getVoxelIndex(int x, int y, int z) const;
    bool isValidCoordinate(int x, int y, int z) const;
    void getCoordinates(int index, int& x, int& y, int& z) const;
};

#endif // IMPROVEDNEARESTNEIGHBORASSIGNER_H