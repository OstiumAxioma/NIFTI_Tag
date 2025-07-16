#ifndef NIFTIVISUALIZATIONAPI_H
#define NIFTIVISUALIZATIONAPI_H

#include <QObject>
#include <QList>
#include <QString>
#include <QColor>
#include <functional>

// VTK前向声明
class vtkRenderer;
class vtkImageData;

/**
 * @brief NIFTI脑影像可视化静态库API
 * 
 * 这是唯一的对外接口，封装了所有NIFTI脑影像与标签可视化功能。
 * 
 * 主要功能：
 * - MRI NIFTI文件的灰度体绘制
 * - 脑区标签NIFTI文件的分区着色
 * - 标签与MRI融合显示
 * - 多标签区块独立显示/隐藏控制
 * - 自动渲染顺序调整
 * 
 * 使用示例：
 * @code
 * NiftiVisualizationAPI* api = new NiftiVisualizationAPI();
 * api->setRenderer(yourVTKRenderer);
 * api->loadMriNifti("path/to/mri.nii");
 * api->loadLabelNifti("path/to/labels.nii");
 * api->processRegions();
 * @endcode
 */
class NiftiVisualizationAPI : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit NiftiVisualizationAPI(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~NiftiVisualizationAPI();

    // ========== 渲染器设置 ==========
    
    /**
     * @brief 设置VTK渲染器
     * @param renderer VTK渲染器指针
     * @note 必须在使用其他功能前设置
     */
    void setRenderer(vtkRenderer* renderer);
    
    /**
     * @brief 获取当前渲染器
     * @return VTK渲染器指针
     */
    vtkRenderer* getRenderer() const;

    // ========== 文件加载 ==========
    
    /**
     * @brief 加载MRI NIFTI文件
     * @param filePath NIFTI文件路径（.nii或.nii.gz）
     * @return 加载成功返回true，失败返回false
     */
    bool loadMriNifti(const QString& filePath);
    
    /**
     * @brief 加载脑区标签NIFTI文件
     * @param filePath NIFTI文件路径（.nii或.nii.gz）
     * @return 加载成功返回true，失败返回false
     */
    bool loadLabelNifti(const QString& filePath);

    // ========== 数据处理 ==========
    
    /**
     * @brief 处理脑区块并生成可视化
     * @note 需要先加载MRI和标签数据
     */
    void processRegions();
    
    /**
     * @brief 处理脑区块并生成可视化（带灰度值限制）
     * @param minGrayValue 最小灰度值限制
     * @param maxGrayValue 最大灰度值限制
     * @note 需要先加载MRI和标签数据
     */
    void processRegions(double minGrayValue, double maxGrayValue);
    
    /**
     * @brief 清理所有区块数据
     */
    void clearRegions();

    // ========== 区块控制 ==========
    
    /**
     * @brief 设置指定区块的可见性
     * @param label 区块标签编号
     * @param visible 是否可见
     */
    void setRegionVisibility(int label, bool visible);
    
    /**
     * @brief 设置所有区块的可见性
     * @param visible 是否可见
     */
    void setAllRegionsVisibility(bool visible);
    
    /**
     * @brief 根据相机距离排序Volume渲染顺序
     * @note 解决VTK 8.2多体渲染bug
     */
    void sortVolumesByCamera();
    
    /**
     * @brief 设置指定区块的颜色
     * @param label 区块标签编号
     * @param color 新颜色
     */
    void setRegionColor(int label, const QColor& color);
    
    /**
     * @brief 设置指定区块的不透明度
     * @param label 区块标签编号
     * @param opacity 不透明度（0.0-1.0）
     */
    void setRegionOpacity(int label, double opacity);
    
    /**
     * @brief 设置所有区块的灰度值限制
     * @param minGrayValue 最小灰度值限制
     * @param maxGrayValue 最大灰度值限制
     * @note 用于适应不同MRI代表脑实质的灰度值不同的情况
     */
    void setGrayValueLimits(double minGrayValue, double maxGrayValue);

    // ========== 信息获取 ==========
    
    /**
     * @brief 获取所有区块标签编号
     * @return 标签编号列表
     */
    QList<int> getAllLabels() const;
    
    /**
     * @brief 获取指定区块的颜色
     * @param label 区块标签编号
     * @return 区块颜色
     */
    QColor getRegionColor(int label) const;
    
    /**
     * @brief 获取指定区块的可见性
     * @param label 区块标签编号
     * @return 是否可见
     */
    bool isRegionVisible(int label) const;
    
    /**
     * @brief 获取指定区块的不透明度
     * @param label 区块标签编号
     * @return 不透明度（0.0-1.0）
     */
    double getRegionOpacity(int label) const;

    // ========== 状态查询 ==========
    
    /**
     * @brief 检查是否已加载MRI数据
     * @return 已加载返回true
     */
    bool hasMriData() const;
    
    /**
     * @brief 检查是否已加载标签数据
     * @return 已加载返回true
     */
    bool hasLabelData() const;
    
    /**
     * @brief 获取区块数量
     * @return 区块数量
     */
    int getRegionCount() const;
    
    /**
     * @brief 检查是否已处理区块
     * @return 已处理返回true
     */
    bool hasProcessedRegions() const;

    // ========== 回调设置 ==========
    
    /**
     * @brief 设置错误回调函数
     * @param callback 错误回调函数
     */
    void setErrorCallback(std::function<void(const QString&)> callback);
    
    /**
     * @brief 设置区块处理完成回调函数
     * @param callback 处理完成回调函数
     */
    void setRegionsProcessedCallback(std::function<void()> callback);
    
    /**
     * @brief 设置区块可见性变化回调函数
     * @param callback 可见性变化回调函数
     */
    void setRegionVisibilityCallback(std::function<void(int, bool)> callback);

    // ========== 高级功能 ==========
    
    /**
     * @brief 重置相机视角
     */
    void resetCamera();
    
    /**
     * @brief 刷新渲染
     */
    void render();
    
    /**
     * @brief 导出区块信息到文件
     * @param filePath 导出文件路径
     * @return 导出成功返回true
     */
    bool exportRegionInfo(const QString& filePath) const;
    
    /**
     * @brief 测试简单的体绘制功能
     * @note 用于验证基础NIFTI体绘制是否正常工作
     */
    void testSimpleVolumeRendering();
    
    /**
     * @brief 预览MRI数据的可视化效果
     * @note 使用当前设置的灰度值限制进行预览，用于调整灰度值参数
     */
    void previewMriVisualization();

signals:
    /**
     * @brief 错误发生信号
     * @param message 错误信息
     */
    void errorOccurred(const QString& message);
    
    /**
     * @brief 区块处理完成信号
     */
    void regionsProcessed();
    
    /**
     * @brief 区块可见性变化信号
     * @param label 区块标签编号
     * @param visible 是否可见
     */
    void regionVisibilityChanged(int label, bool visible);

private:
    class NiftiVisualizationAPIPrivate;
    NiftiVisualizationAPIPrivate* d_ptr;
    Q_DECLARE_PRIVATE(NiftiVisualizationAPI)
    
    // 私有辅助方法
    void renderSingleVolume(vtkImageData* imageData, const QColor& color, const QString& name);
};

#endif // NIFTIVISUALIZATIONAPI_H 