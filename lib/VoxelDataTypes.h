#ifndef VOXELDATATYPES_H
#define VOXELDATATYPES_H

/**
 * @brief 共享的体素数据结构定义
 * 
 * 避免在多个类中重复定义相同的数据结构
 */
struct VoxelData {
    float grayValue;    // MRI灰度值
    int label;          // 标签值
    
    VoxelData() : grayValue(0.0f), label(0) {}
    VoxelData(float gray, int lbl) : grayValue(gray), label(lbl) {}
    
    // 比较运算符
    bool operator==(const VoxelData& other) const {
        return grayValue == other.grayValue && label == other.label;
    }
    
    bool operator!=(const VoxelData& other) const {
        return !(*this == other);
    }
};

#endif // VOXELDATATYPES_H