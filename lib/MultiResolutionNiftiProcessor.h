#ifndef MULTIRESOLUTIONNIFTIPROCESSOR_H
#define MULTIRESOLUTIONNIFTIPROCESSOR_H

#include <QObject>
#include <QString>
#include <QDebug>

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkImageInterpolator.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkNIFTIImageReader.h>
#include <vtkImageCast.h>
#include <vtkImageThreshold.h>
#include <vtkImageMathematics.h>

/**
 * @brief 多分辨率NIFTI处理器
 * 
 * 专门处理不同分辨率的MRI和标签数据融合问题
 * 支持：
 * - 高分辨率MRI数据
 * - 低分辨率Brodmann标签数据
 * - 自动空间配准和重采样
 * - 标签精确保持（最近邻插值）
 * 
 * 使用示例：
 * @code
 * MultiResolutionNiftiProcessor processor;
 * processor.loadHighResMRI("path/to/high_res_mri.nii");
 * processor.loadLowResLabels("path/to/brodmann_labels.nii");
 * vtkImageData* alignedLabels = processor.alignLabelsToMRI();
 * @endcode
 */
class MultiResolutionNiftiProcessor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 重采样模式
     */
    enum ResampleMode {
        LABELS_TO_MRI,    // 标签数据上采样到MRI分辨率（推荐）
        MRI_TO_LABELS,    // MRI数据下采样到标签分辨率
        CUSTOM_RESOLUTION // 自定义目标分辨率
    };

    /**
     * @brief 插值方法
     */
    enum InterpolationMethod {
        NEAREST_NEIGHBOR, // 最近邻插值（适用于标签数据）
        LINEAR,          // 线性插值（适用于MRI数据）
        CUBIC           // 三次插值（高质量MRI重采样）
    };

    explicit MultiResolutionNiftiProcessor(QObject *parent = nullptr);
    ~MultiResolutionNiftiProcessor();

    // ========== 数据加载 ==========
    
    /**
     * @brief 加载高分辨率MRI数据
     * @param filePath MRI NIFTI文件路径
     * @return 加载成功返回true
     */
    bool loadHighResMRI(const QString& filePath);
    
    /**
     * @brief 加载低分辨率标签数据
     * @param filePath 标签NIFTI文件路径（如Brodmann_space-MNI152NLin6_res-1x1x1.nii）
     * @return 加载成功返回true
     */
    bool loadLowResLabels(const QString& filePath);

    // ========== 空间配准与重采样 ==========
    
    /**
     * @brief 将标签数据对齐到MRI空间
     * @param interpolationMethod 插值方法（默认最近邻）
     * @return 对齐后的标签数据
     */
    vtkSmartPointer<vtkImageData> alignLabelsToMRI(
        InterpolationMethod interpolationMethod = NEAREST_NEIGHBOR);
    
    /**
     * @brief 将MRI数据对齐到标签空间
     * @param interpolationMethod 插值方法（默认线性）
     * @return 对齐后的MRI数据
     */
    vtkSmartPointer<vtkImageData> alignMRIToLabels(
        InterpolationMethod interpolationMethod = LINEAR);
    
    /**
     * @brief 重采样到自定义分辨率
     * @param targetSpacing 目标体素间距
     * @param targetDimensions 目标图像尺寸
     * @param sourceData 源数据
     * @param interpolationMethod 插值方法
     * @return 重采样后的数据
     */
    vtkSmartPointer<vtkImageData> resampleToCustomResolution(
        double targetSpacing[3], 
        int targetDimensions[3],
        vtkImageData* sourceData,
        InterpolationMethod interpolationMethod = NEAREST_NEIGHBOR);

    // ========== 空间变换 ==========
    
    /**
     * @brief 设置自定义变换矩阵
     * @param matrix 4x4变换矩阵
     * @note 用于需要特殊空间配准的情况
     */
    void setCustomTransform(vtkMatrix4x4* matrix);
    
    /**
     * @brief 自动计算配准变换
     * @return 计算成功返回true
     * @note 基于图像中心和体素间距进行简单配准
     */
    bool computeAutoAlignment();

    // ========== 质量控制 ==========
    
    /**
     * @brief 验证数据空间一致性
     * @return 空间一致返回true
     */
    bool validateSpatialConsistency();
    
    /**
     * @brief 输出详细的空间信息
     */
    void printSpatialInfo() const;
    
    /**
     * @brief 检查标签完整性
     * @param originalLabels 原始标签数据
     * @param resampledLabels 重采样后标签数据
     * @return 标签保持完整返回true
     */
    bool validateLabelIntegrity(vtkImageData* originalLabels, 
                               vtkImageData* resampledLabels);

    // ========== 数据获取 ==========
    
    /**
     * @brief 获取原始MRI数据
     */
    vtkImageData* getOriginalMRI() const { return originalMRI; }
    
    /**
     * @brief 获取原始标签数据
     */
    vtkImageData* getOriginalLabels() const { return originalLabels; }
    
    /**
     * @brief 获取处理后的MRI数据
     */
    vtkImageData* getProcessedMRI() const { return processedMRI; }
    
    /**
     * @brief 获取处理后的标签数据
     */
    vtkImageData* getProcessedLabels() const { return processedLabels; }

    // ========== 工具方法 ==========
    
    /**
     * @brief 创建标签掩码
     * @param labelData 标签数据
     * @param targetLabel 目标标签值
     * @return 二值化掩码
     */
    static vtkSmartPointer<vtkImageData> createLabelMask(
        vtkImageData* labelData, int targetLabel);
    
    /**
     * @brief 应用掩码到MRI数据
     * @param mriData MRI数据
     * @param maskData 掩码数据
     * @return 掩码后的MRI数据
     */
    static vtkSmartPointer<vtkImageData> applyMaskToMRI(
        vtkImageData* mriData, vtkImageData* maskData);
    
    /**
     * @brief 计算两个数据集的空间重叠度
     * @param data1 数据集1
     * @param data2 数据集2
     * @return 重叠度（0-1）
     */
    static double calculateSpatialOverlap(vtkImageData* data1, vtkImageData* data2);

signals:
    void processingStarted();
    void processingProgress(int percentage);
    void processingCompleted();
    void errorOccurred(const QString& message);

private:
    // 原始数据
    vtkSmartPointer<vtkImageData> originalMRI;
    vtkSmartPointer<vtkImageData> originalLabels;
    
    // 处理后数据
    vtkSmartPointer<vtkImageData> processedMRI;
    vtkSmartPointer<vtkImageData> processedLabels;
    
    // 变换矩阵
    vtkSmartPointer<vtkMatrix4x4> customTransform;
    
    // 空间信息
    struct SpatialInfo {
        double spacing[3];
        double origin[3];
        int dimensions[3];
        double bounds[6];
        
        void print(const QString& name) const {
            qDebug() << name << "空间信息:";
            qDebug() << "  体素间距:" << spacing[0] << "x" << spacing[1] << "x" << spacing[2];
            qDebug() << "  图像尺寸:" << dimensions[0] << "x" << dimensions[1] << "x" << dimensions[2];
            qDebug() << "  原点坐标:" << origin[0] << "," << origin[1] << "," << origin[2];
            qDebug() << "  边界范围:" << bounds[0] << "~" << bounds[1] << "," 
                     << bounds[2] << "~" << bounds[3] << "," << bounds[4] << "~" << bounds[5];
        }
    };
    
    SpatialInfo mriSpatialInfo;
    SpatialInfo labelSpatialInfo;
    
    // 私有方法
    void extractSpatialInfo(vtkImageData* data, SpatialInfo& info);
    vtkSmartPointer<vtkImageReslice> createResliceFilter(
        vtkImageData* sourceData, 
        const SpatialInfo& targetInfo,
        InterpolationMethod interpolationMethod);
    bool loadNiftiFile(const QString& filePath, vtkSmartPointer<vtkImageData>& output);
    void setupInterpolation(vtkImageReslice* reslice, InterpolationMethod method);
    QList<int> extractUniqueLabels(vtkImageData* labelData);
    void validateResampledLabels(vtkImageData* original, vtkImageData* resampled);
};

#endif // MULTIRESOLUTIONNIFTIPROCESSOR_H