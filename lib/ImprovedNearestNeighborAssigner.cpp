#include "ImprovedNearestNeighborAssigner.h"
#include <algorithm>
#include <cmath>
#include <queue>

ImprovedNearestNeighborAssigner::ImprovedNearestNeighborAssigner(int* dims, double* spacingData)
{
    dimensions[0] = dims[0];
    dimensions[1] = dims[1];
    dimensions[2] = dims[2];
    spacing[0] = spacingData[0];
    spacing[1] = spacingData[1];
    spacing[2] = spacingData[2];
    
    qDebug() << "改进的最近邻分配器初始化：" 
             << dimensions[0] << "x" << dimensions[1] << "x" << dimensions[2];
}

int ImprovedNearestNeighborAssigner::assignNearestNeighborLabels(
    QVector<VoxelData>& fusedData, const QVector<int>& uniqueLabels)
{
    qDebug() << "开始改进的最近邻标签分配...";
    
    int assignedCount = 0;
    int totalVoxels = fusedData.size();
    int processedCount = 0;
    
    for (int i = 0; i < totalVoxels; i++) {
        VoxelData& voxel = fusedData[i];
        
        // 只处理无标签但有意义的MRI体素
        if (voxel.label == 0 && voxel.grayValue > 0.0f) {
            // 将线性索引转换为3D坐标
            int x, y, z;
            getCoordinates(i, x, y, z);
            
            int bestLabel = findBestMatchingLabel(x, y, z, voxel.grayValue, fusedData);
            if (bestLabel > 0) {
                voxel.label = bestLabel;
                assignedCount++;
            }
            
            processedCount++;
            
            // 每处理1000个体素输出一次进度
            if (processedCount % 1000 == 0) {
                qDebug() << "已处理" << processedCount << "个无标签体素，分配了" << assignedCount << "个";
            }
        }
    }
    
    qDebug() << "改进的最近邻标签分配完成：";
    qDebug() << "- 处理了" << processedCount << "个无标签体素";
    qDebug() << "- 成功分配了" << assignedCount << "个体素";
    qDebug() << "- 分配成功率：" << (processedCount > 0 ? (assignedCount * 100.0 / processedCount) : 0) << "%";
    
    return assignedCount;
}

int ImprovedNearestNeighborAssigner::findBestMatchingLabel(
    int x, int y, int z, float targetGray, const QVector<VoxelData>& fusedData)
{
    // 策略1：优先队列搜索（基于距离和灰度相似性）
    int label1 = priorityQueueSearch(x, y, z, targetGray, fusedData);
    if (label1 > 0) {
        return label1;
    }
    
    // 策略2：扩大搜索范围的传统方法
    const int maxRadius = 25; // 扩大搜索半径
    
    for (int radius = 1; radius <= maxRadius; radius++) {
        std::vector<SearchCandidate> candidates;
        
        // 收集当前半径内的所有候选者
        for (int dz = -radius; dz <= radius; dz++) {
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;
                    
                    if (isValidCoordinate(nx, ny, nz)) {
                        int index = getVoxelIndex(nx, ny, nz);
                        if (index >= 0 && index < fusedData.size()) {
                            const VoxelData& neighbor = fusedData[index];
                            if (neighbor.label > 0) {
                                float distance = calculateDistance(x, y, z, nx, ny, nz);
                                float graySimilarity = calculateGraySimilarity(targetGray, neighbor.grayValue);
                                
                                candidates.push_back({
                                    nx, ny, nz, 
                                    neighbor.label, 
                                    distance, 
                                    neighbor.grayValue
                                });
                            }
                        }
                    }
                }
            }
        }
        
        // 如果找到候选者，选择最佳匹配
        if (!candidates.empty()) {
            // 根据综合分数排序
            std::sort(candidates.begin(), candidates.end(), 
                [this, targetGray](const SearchCandidate& a, const SearchCandidate& b) {
                    float scoreA = calculateMatchScore(a.distance, 
                        calculateGraySimilarity(targetGray, a.grayValue));
                    float scoreB = calculateMatchScore(b.distance, 
                        calculateGraySimilarity(targetGray, b.grayValue));
                    return scoreA > scoreB; // 分数高的优先
                });
            
            return candidates[0].label;
        }
    }
    
    // 策略3：如果仍然找不到，返回0（保持无标签状态）
    return 0;
}

int ImprovedNearestNeighborAssigner::priorityQueueSearch(
    int x, int y, int z, float targetGray, const QVector<VoxelData>& fusedData)
{
    std::priority_queue<SearchCandidate> candidates;
    const int maxRadius = 15;
    const int maxCandidates = 50; // 限制候选者数量以提高性能
    
    // 收集候选者
    for (int radius = 1; radius <= maxRadius; radius++) {
        for (int dz = -radius; dz <= radius; dz++) {
            for (int dy = -radius; dy <= radius; dy++) {
                for (int dx = -radius; dx <= radius; dx++) {
                    int nx = x + dx;
                    int ny = y + dy;
                    int nz = z + dz;
                    
                    if (isValidCoordinate(nx, ny, nz)) {
                        int index = getVoxelIndex(nx, ny, nz);
                        if (index >= 0 && index < fusedData.size()) {
                            const VoxelData& neighbor = fusedData[index];
                            if (neighbor.label > 0) {
                                float distance = calculateDistance(x, y, z, nx, ny, nz);
                                
                                candidates.push({
                                    nx, ny, nz,
                                    neighbor.label,
                                    distance,
                                    neighbor.grayValue
                                });
                                
                                // 限制队列大小
                                if (candidates.size() > maxCandidates) {
                                    candidates.pop();
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 如果已经找到足够的候选者，可以提前结束
        if (candidates.size() >= 10) {
            break;
        }
    }
    
    // 选择最佳候选者
    if (!candidates.empty()) {
        SearchCandidate best = candidates.top();
        float bestScore = calculateMatchScore(best.distance, 
            calculateGraySimilarity(targetGray, best.grayValue));
        
        // 检查前几个候选者，选择综合分数最高的
        std::vector<SearchCandidate> topCandidates;
        while (!candidates.empty() && topCandidates.size() < 5) {
            topCandidates.push_back(candidates.top());
            candidates.pop();
        }
        
        int bestLabel = 0;
        float maxScore = 0;
        
        for (const auto& candidate : topCandidates) {
            float score = calculateMatchScore(candidate.distance,
                calculateGraySimilarity(targetGray, candidate.grayValue));
            if (score > maxScore) {
                maxScore = score;
                bestLabel = candidate.label;
            }
        }
        
        return bestLabel;
    }
    
    return 0;
}

float ImprovedNearestNeighborAssigner::calculateGraySimilarity(float gray1, float gray2)
{
    float diff = std::abs(gray1 - gray2);
    float maxGray = std::max(gray1, gray2);
    
    if (maxGray < 1.0f) {
        return 1.0f; // 都接近0，认为相似
    }
    
    // 相对差异越小，相似度越高
    float relativeDiff = diff / maxGray;
    return std::exp(-relativeDiff * 2.0f); // 指数衰减函数
}

float ImprovedNearestNeighborAssigner::calculateMatchScore(float distance, float graySimilarity)
{
    // 综合分数：距离权重0.3，灰度相似性权重0.7
    float distanceScore = std::exp(-distance * 0.1f); // 距离越近分数越高
    return 0.3f * distanceScore + 0.7f * graySimilarity;
}

bool ImprovedNearestNeighborAssigner::isInSameRegion(
    int x1, int y1, int z1, int x2, int y2, int z2, const QVector<VoxelData>& fusedData)
{
    // 简单的连通性检查：检查路径上的体素是否都有合理的灰度值
    int steps = std::max({std::abs(x2-x1), std::abs(y2-y1), std::abs(z2-z1)});
    if (steps == 0) return true;
    
    for (int i = 1; i < steps; i++) {
        float t = float(i) / steps;
        int mx = x1 + int(t * (x2 - x1));
        int my = y1 + int(t * (y2 - y1));
        int mz = z1 + int(t * (z2 - z1));
        
        if (isValidCoordinate(mx, my, mz)) {
            int index = getVoxelIndex(mx, my, mz);
            if (index >= 0 && index < fusedData.size()) {
                if (fusedData[index].grayValue < 1.0f) {
                    return false; // 路径中断
                }
            }
        }
    }
    
    return true;
}

float ImprovedNearestNeighborAssigner::calculateDistance(int x1, int y1, int z1, int x2, int y2, int z2)
{
    float dx = (x1 - x2) * spacing[0];
    float dy = (y1 - y2) * spacing[1];
    float dz = (z1 - z2) * spacing[2];
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

int ImprovedNearestNeighborAssigner::getVoxelIndex(int x, int y, int z) const
{
    return z * dimensions[0] * dimensions[1] + y * dimensions[0] + x;
}

bool ImprovedNearestNeighborAssigner::isValidCoordinate(int x, int y, int z) const
{
    return x >= 0 && x < dimensions[0] &&
           y >= 0 && y < dimensions[1] &&
           z >= 0 && z < dimensions[2];
}

void ImprovedNearestNeighborAssigner::getCoordinates(int index, int& x, int& y, int& z) const
{
    z = index / (dimensions[0] * dimensions[1]);
    y = (index % (dimensions[0] * dimensions[1])) / dimensions[0];
    x = index % dimensions[0];
}