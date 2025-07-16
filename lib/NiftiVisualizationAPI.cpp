#include "../api/NiftiVisualizationAPI.h"
#include "niftimanager.h"
#include "brainregionvolume.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkMarchingCubes.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkActorCollection.h>

/**
 * @brief NiftiVisualizationAPI的私有实现类
 * 
 * 使用PIMPL模式隐藏内部实现细节，保持API接口稳定
 */
class NiftiVisualizationAPI::NiftiVisualizationAPIPrivate
{
public:
    NiftiVisualizationAPIPrivate(NiftiVisualizationAPI* q)
        : q_ptr(q)
        , niftiManager(nullptr)
        , renderer(nullptr)
        , currentMinGrayValue(0.0)
        , currentMaxGrayValue(0.0)
        , useGrayValueLimits(false)
    {
        // 创建内部NIFTI管理器
        niftiManager = new NiftiManager(q);
        
        // 连接内部信号到API信号
        QObject::connect(niftiManager, &NiftiManager::errorOccurred,
                        q, &NiftiVisualizationAPI::errorOccurred);
        QObject::connect(niftiManager, &NiftiManager::regionsProcessed,
                        q, &NiftiVisualizationAPI::regionsProcessed);
        QObject::connect(niftiManager, &NiftiManager::regionVisibilityChanged,
                        q, &NiftiVisualizationAPI::regionVisibilityChanged);
    }
    
    ~NiftiVisualizationAPIPrivate()
    {
        // 清理MRI预览actor
        if (mriPreviewActor && renderer) {
            renderer->RemoveActor(mriPreviewActor);
            mriPreviewActor = nullptr;
        }
        
        if (niftiManager) {
            delete niftiManager;
        }
    }
    
    // 成员变量
    NiftiVisualizationAPI* q_ptr;
    NiftiManager* niftiManager;
    vtkRenderer* renderer;
    
    // 回调函数
    std::function<void(const QString&)> errorCallback;
    std::function<void()> regionsProcessedCallback;
    std::function<void(int, bool)> regionVisibilityCallback;
    
    // 灰度值限制
    double currentMinGrayValue;
    double currentMaxGrayValue;
    bool useGrayValueLimits;
    
    // MRI预览actor
    vtkSmartPointer<vtkActor> mriPreviewActor;
    
    Q_DECLARE_PUBLIC(NiftiVisualizationAPI)
};

// ========== 构造与析构 ==========

NiftiVisualizationAPI::NiftiVisualizationAPI(QObject *parent)
    : QObject(parent)
    , d_ptr(new NiftiVisualizationAPIPrivate(this))
{
    Q_D(NiftiVisualizationAPI);
    
    // 连接回调函数
    connect(this, &NiftiVisualizationAPI::errorOccurred, [d](const QString& message) {
        if (d->errorCallback) {
            d->errorCallback(message);
        }
    });
    
    connect(this, &NiftiVisualizationAPI::regionsProcessed, [d]() {
        if (d->regionsProcessedCallback) {
            d->regionsProcessedCallback();
        }
    });
    
    connect(this, &NiftiVisualizationAPI::regionVisibilityChanged, [d](int label, bool visible) {
        if (d->regionVisibilityCallback) {
            d->regionVisibilityCallback(label, visible);
        }
    });
    
    qDebug() << "NiftiVisualizationAPI 初始化";
}

NiftiVisualizationAPI::~NiftiVisualizationAPI()
{
    delete d_ptr;
    qDebug() << "NiftiVisualizationAPI 析构";
}

// ========== 渲染器设置 ==========

void NiftiVisualizationAPI::setRenderer(vtkRenderer* renderer)
{
    Q_D(NiftiVisualizationAPI);
    d->renderer = renderer;
    d->niftiManager->setRenderer(renderer);
}

vtkRenderer* NiftiVisualizationAPI::getRenderer() const
{
    Q_D(const NiftiVisualizationAPI);
    return d->renderer;
}

// ========== 文件加载 ==========

bool NiftiVisualizationAPI::loadMriNifti(const QString& filePath)
{
    Q_D(NiftiVisualizationAPI);
    return d->niftiManager->loadMriNifti(filePath);
}

bool NiftiVisualizationAPI::loadLabelNifti(const QString& filePath)
{
    Q_D(NiftiVisualizationAPI);
    return d->niftiManager->loadLabelNifti(filePath);
}

// ========== 数据处理 ==========

void NiftiVisualizationAPI::processRegions()
{
    Q_D(NiftiVisualizationAPI);
    d->niftiManager->processRegions();
    
    // 处理完成后，确保所有surface actor都已添加到渲染器
    if (d->renderer) {
        QList<int> labels = d->niftiManager->getAllLabels();
        for (int label : labels) {
            auto* volume = d->niftiManager->getRegionVolume(label);
            if (volume) {
                d->renderer->AddActor(volume->getSurfaceActor());
                d->renderer->AddActor(volume->getCentroidSphere());
            }
        }
        
        // 重置相机并渲染
        d->renderer->ResetCamera();
        if (d->renderer->GetRenderWindow()) {
            d->renderer->GetRenderWindow()->Render();
        }
    }
    
    qDebug() << "区块处理完成，surface渲染已添加";
}

void NiftiVisualizationAPI::processRegions(double minGrayValue, double maxGrayValue)
{
    Q_D(NiftiVisualizationAPI);
    
    // 更新当前的灰度值限制
    d->currentMinGrayValue = minGrayValue;
    d->currentMaxGrayValue = maxGrayValue;
    d->useGrayValueLimits = (minGrayValue < maxGrayValue);
    
    d->niftiManager->processRegions(minGrayValue, maxGrayValue);
    
    // 处理完成后，确保所有surface actor都已添加到渲染器
    if (d->renderer) {
        QList<int> labels = d->niftiManager->getAllLabels();
        for (int label : labels) {
            auto* volume = d->niftiManager->getRegionVolume(label);
            if (volume) {
                d->renderer->AddActor(volume->getSurfaceActor());
                d->renderer->AddActor(volume->getCentroidSphere());
            }
        }
        
        // 重置相机并渲染
        d->renderer->ResetCamera();
        if (d->renderer->GetRenderWindow()) {
            d->renderer->GetRenderWindow()->Render();
        }
    }
    
    qDebug() << "区块处理完成（带灰度值限制），surface渲染已添加";
}

void NiftiVisualizationAPI::testSimpleVolumeRendering()
{
    Q_D(NiftiVisualizationAPI);
    
    if (!d->renderer) {
        qDebug() << "渲染器未设置，无法进行体绘制测试";
        return;
    }
    
    qDebug() << "开始简单体绘制测试";
    
    try {
        // 清理之前的渲染对象
        d->renderer->RemoveAllViewProps();
        
        // 测试MRI数据
        if (d->niftiManager->hasMriData()) {
            qDebug() << "开始渲染MRI数据";
            renderSingleVolume(d->niftiManager->getMriImage(), QColor(255, 255, 255), "MRI");
        }
        
        // 测试标签数据
        if (d->niftiManager->hasLabelData()) {
            qDebug() << "开始渲染标签数据";
            renderSingleVolume(d->niftiManager->getLabelImage(), QColor(255, 0, 0), "Label");
        }
        
        // 重置相机并渲染
        d->renderer->ResetCamera();
        if (d->renderer->GetRenderWindow()) {
            d->renderer->GetRenderWindow()->Render();
        }
        
        qDebug() << "简单体绘制测试完成";
    }
    catch (const std::exception& e) {
        qDebug() << "体绘制测试失败:" << e.what();
    }
    catch (...) {
        qDebug() << "体绘制测试失败: 未知错误";
    }
}

void NiftiVisualizationAPI::previewMriVisualization()
{
    Q_D(NiftiVisualizationAPI);
    
    if (!d->renderer) {
        qDebug() << "渲染器未设置，无法进行MRI预览";
        return;
    }
    
    if (!d->niftiManager->hasMriData()) {
        qDebug() << "没有MRI数据，无法进行预览";
        return;
    }
    
    qDebug() << "开始MRI预览，使用灰度值限制: [" << d->currentMinGrayValue << ", " << d->currentMaxGrayValue << "]";
    
    try {
        // 清理之前的MRI预览actor
        if (d->mriPreviewActor) {
            d->renderer->RemoveActor(d->mriPreviewActor);
            d->mriPreviewActor = nullptr;
        }
        
        // 创建新的MRI预览actor（使用智能指针确保生命周期管理）
        d->mriPreviewActor = vtkSmartPointer<vtkActor>::New();
        
        if (createMriPreviewActor(d->niftiManager->getMriImage(), d->mriPreviewActor)) {
            // 添加到渲染器
            d->renderer->AddActor(d->mriPreviewActor);
        } else {
            qDebug() << "MRI预览actor创建失败";
            d->mriPreviewActor = nullptr;
        }
        
        // 重置相机并渲染
        d->renderer->ResetCamera();
        if (d->renderer->GetRenderWindow()) {
            d->renderer->GetRenderWindow()->Render();
        }
        
        qDebug() << "MRI预览完成";
    }
    catch (const std::exception& e) {
        qDebug() << "MRI预览失败:" << e.what();
    }
    catch (...) {
        qDebug() << "MRI预览失败: 未知错误";
    }
}

void NiftiVisualizationAPI::renderSingleVolume(vtkImageData* imageData, const QColor& color, const QString& name)
{
    NiftiVisualizationAPIPrivate* d = d_func();
    
    if (!imageData || !d->renderer) {
        qDebug() << "数据或渲染器为空，跳过" << name << "渲染";
        return;
    }
    
    try {
        qDebug() << "开始渲染" << name << "数据 (使用surface渲染)";
        
        // 获取数据范围以设置合适的阈值
        double* range = imageData->GetScalarRange();
        qDebug() << name << "数据范围: [" << range[0] << ", " << range[1] << "]";
        
        // 应用灰度值限制
        double effectiveMinValue = range[0];
        double effectiveMaxValue = range[1];
        
        if (d->useGrayValueLimits) {
            effectiveMinValue = std::max(range[0], d->currentMinGrayValue);
            effectiveMaxValue = std::min(range[1], d->currentMaxGrayValue);
            qDebug() << name << "应用灰度值限制: [" << effectiveMinValue << ", " << effectiveMaxValue << "]";
        }
        
        // 使用Marching Cubes算法生成等值面
        auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
        marchingCubes->SetInputData(imageData);
        
        // 改进的阈值算法，类似于BrainRegionVolume中的实现
        double threshold;
        double dataRange = effectiveMaxValue - effectiveMinValue;
        
        if (dataRange > 0) {
            // 根据数据范围选择合适的阈值百分比
            if (dataRange > 1000) {
                // 高动态范围数据，使用中等阈值
                threshold = effectiveMinValue + dataRange * 0.35;
            } else if (dataRange > 100) {
                // 中等动态范围数据，使用低阈值
                threshold = effectiveMinValue + dataRange * 0.15;
            } else {
                // 低动态范围数据，使用非常低的阈值
                threshold = effectiveMinValue + dataRange * 0.05;
            }
        } else {
            // 回退到简单阈值
            threshold = effectiveMinValue + 0.1;
        }
        
        marchingCubes->SetValue(0, threshold);
        qDebug() << name << "使用阈值: " << threshold << "(数据范围: " << dataRange << ")";
        
        marchingCubes->Update();
        
        // 创建mapper
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(marchingCubes->GetOutputPort());
        
        // 创建actor
        auto actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);
        
        // 设置材质属性
        actor->GetProperty()->SetColor(color.redF(), color.greenF(), color.blueF());
        actor->GetProperty()->SetOpacity(1.0);  // 完全不透明
        actor->GetProperty()->SetAmbient(0.3);   // 环境光
        actor->GetProperty()->SetDiffuse(0.7);   // 漫反射
        actor->GetProperty()->SetSpecular(0.2);  // 镜面反射
        actor->GetProperty()->SetSpecularPower(10);
        
        // 添加到渲染器
        d->renderer->AddActor(actor);
        
        qDebug() << name << "数据渲染完成 (surface渲染)";
    }
    catch (const std::exception& e) {
        qDebug() << name << "渲染失败:" << e.what();
    }
    catch (...) {
        qDebug() << name << "渲染失败: 未知错误";
    }
}

bool NiftiVisualizationAPI::createMriPreviewActor(vtkImageData* imageData, vtkSmartPointer<vtkActor> actor)
{
    Q_D(NiftiVisualizationAPI);
    
    if (!imageData || !actor) {
        qDebug() << "MRI数据或actor为空，无法创建预览actor";
        return false;
    }
    
    try {
        // 获取数据范围以设置合适的阈值
        double* range = imageData->GetScalarRange();
        qDebug() << "MRI预览数据范围: [" << range[0] << ", " << range[1] << "]";
        
        // 应用灰度值限制
        double effectiveMinValue = range[0];
        double effectiveMaxValue = range[1];
        
        if (d->useGrayValueLimits) {
            effectiveMinValue = std::max(range[0], d->currentMinGrayValue);
            effectiveMaxValue = std::min(range[1], d->currentMaxGrayValue);
            qDebug() << "MRI预览应用灰度值限制: [" << effectiveMinValue << ", " << effectiveMaxValue << "]";
        }
        
        // 使用Marching Cubes算法生成等值面
        auto marchingCubes = vtkSmartPointer<vtkMarchingCubes>::New();
        marchingCubes->SetInputData(imageData);
        
        // 改进的阈值算法
        double threshold;
        double dataRange = effectiveMaxValue - effectiveMinValue;
        
        if (dataRange > 0) {
            // 根据数据范围选择合适的阈值百分比
            if (dataRange > 1000) {
                threshold = effectiveMinValue + dataRange * 0.35;
            } else if (dataRange > 100) {
                threshold = effectiveMinValue + dataRange * 0.15;
            } else {
                threshold = effectiveMinValue + dataRange * 0.05;
            }
        } else {
            threshold = effectiveMinValue + 0.1;
        }
        
        marchingCubes->SetValue(0, threshold);
        qDebug() << "MRI预览使用阈值: " << threshold << "(数据范围: " << dataRange << ")";
        
        try {
            marchingCubes->Update();
            
            // 检查Marching Cubes输出
            vtkPolyData* polyData = marchingCubes->GetOutput();
            if (!polyData) {
                qDebug() << "MRI预览Marching Cubes输出为空";
                return false;
            }
            
            int numPoints = polyData->GetNumberOfPoints();
            int numCells = polyData->GetNumberOfCells();
            qDebug() << "MRI预览Marching Cubes生成了" << numPoints << "个点和" << numCells << "个面";
            
            if (numPoints == 0 || numCells == 0) {
                qDebug() << "MRI预览Marching Cubes没有生成有效几何体，尝试调整阈值";
                
                // 尝试更低的阈值
                double lowerThreshold = effectiveMinValue + dataRange * 0.01;
                marchingCubes->SetValue(0, lowerThreshold);
                qDebug() << "MRI预览尝试更低阈值: " << lowerThreshold;
                
                marchingCubes->Update();
                polyData = marchingCubes->GetOutput();
                
                if (!polyData || polyData->GetNumberOfPoints() == 0) {
                    qDebug() << "MRI预览即使使用更低阈值也无法生成有效几何体";
                    return false;
                }
                
                qDebug() << "MRI预览使用更低阈值生成了" << polyData->GetNumberOfPoints() << "个点";
            }
            
            // 检查几何体是否过于复杂，如果是则调整阈值
            if (numPoints > 100000 || numCells > 200000) {
                qDebug() << "MRI预览几何体过于复杂，尝试提高阈值";
                
                // 提高阈值以减少几何体复杂度
                double higherThreshold = effectiveMinValue + dataRange * 0.8; // 使用80%的阈值
                marchingCubes->SetValue(0, higherThreshold);
                qDebug() << "MRI预览尝试更高阈值: " << higherThreshold;
                
                marchingCubes->Update();
                polyData = marchingCubes->GetOutput();
                
                if (polyData && polyData->GetNumberOfPoints() > 0) {
                    int newNumPoints = polyData->GetNumberOfPoints();
                    int newNumCells = polyData->GetNumberOfCells();
                    qDebug() << "MRI预览使用更高阈值生成了" << newNumPoints << "个点和" << newNumCells << "个面";
                    
                    // 如果仍然太复杂，再次提高阈值
                    if (newNumPoints > 50000 || newNumCells > 100000) {
                        double veryHighThreshold = effectiveMinValue + dataRange * 0.9; // 使用90%的阈值
                        marchingCubes->SetValue(0, veryHighThreshold);
                        qDebug() << "MRI预览尝试非常高的阈值: " << veryHighThreshold;
                        
                        marchingCubes->Update();
                        polyData = marchingCubes->GetOutput();
                        
                        if (polyData && polyData->GetNumberOfPoints() > 0) {
                            qDebug() << "MRI预览最终生成了" << polyData->GetNumberOfPoints() << "个点和" << polyData->GetNumberOfCells() << "个面";
                        }
                    }
                } else {
                    qDebug() << "MRI预览更高阈值无法生成有效几何体，回退到原始阈值";
                    marchingCubes->SetValue(0, threshold);
                    marchingCubes->Update();
                }
            }
            
        } catch (const std::exception& e) {
            qDebug() << "MRI预览Marching Cubes处理异常:" << e.what();
            return false;
        }
        
        // 创建mapper
        auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        if (!mapper) {
            qDebug() << "MRI预览mapper创建失败";
            return false;
        }
        
        try {
            mapper->SetInputConnection(marchingCubes->GetOutputPort());
            mapper->Update();
        } catch (const std::exception& e) {
            qDebug() << "MRI预览mapper设置失败:" << e.what();
            return false;
        }
        
        // 设置actor
        try {
            actor->SetMapper(mapper);
            
            // 设置材质属性（不透明白色）
            auto property = actor->GetProperty();
            if (property) {
                property->SetColor(1.0, 1.0, 1.0);
                property->SetOpacity(1.0);  // 完全不透明
                property->SetAmbient(0.3);
                property->SetDiffuse(0.7);
                property->SetSpecular(0.2);
                property->SetSpecularPower(10);
            }
            
            qDebug() << "MRI预览actor创建成功";
            return true;
        } catch (const std::exception& e) {
            qDebug() << "MRI预览actor设置失败:" << e.what();
            return false;
        }
    }
    catch (const std::exception& e) {
        qDebug() << "MRI预览actor创建失败:" << e.what();
        return false;
    }
    catch (...) {
        qDebug() << "MRI预览actor创建失败: 未知错误";
        return false;
    }
}

void NiftiVisualizationAPI::setMriPreviewVisible(bool visible)
{
    Q_D(NiftiVisualizationAPI);
    
    if (d->mriPreviewActor) {
        d->mriPreviewActor->SetVisibility(visible);
        
        if (d->renderer && d->renderer->GetRenderWindow()) {
            d->renderer->GetRenderWindow()->Render();
        }
        
        qDebug() << "MRI预览可见性设置为:" << visible;
    } else {
        qDebug() << "MRI预览actor不存在，无法设置可见性";
    }
}

void NiftiVisualizationAPI::clearRegions()
{
    Q_D(NiftiVisualizationAPI);
    d->niftiManager->clearRegions();
}

// ========== 区块控制 ==========

void NiftiVisualizationAPI::setRegionVisibility(int label, bool visible)
{
    Q_D(NiftiVisualizationAPI);
    d->niftiManager->updateRegionVisibility(label, visible);
}

void NiftiVisualizationAPI::setAllRegionsVisibility(bool visible)
{
    Q_D(NiftiVisualizationAPI);
    QList<int> labels = d->niftiManager->getAllLabels();
    for (int label : labels) {
        d->niftiManager->updateRegionVisibility(label, visible);
    }
}

void NiftiVisualizationAPI::sortVolumesByCamera()
{
    Q_D(NiftiVisualizationAPI);
    if (d->renderer) {
        d->niftiManager->sortVolumesByCamera(d->renderer->GetActiveCamera());
    }
}

void NiftiVisualizationAPI::setRegionColor(int label, const QColor& color)
{
    Q_D(NiftiVisualizationAPI);
    BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
    if (volume) {
        volume->updateColor(color);
    }
}

void NiftiVisualizationAPI::setRegionOpacity(int label, double opacity)
{
    Q_D(NiftiVisualizationAPI);
    BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
    if (volume) {
        volume->setOpacity(opacity);
    }
}

void NiftiVisualizationAPI::setGrayValueLimits(double minGrayValue, double maxGrayValue)
{
    Q_D(NiftiVisualizationAPI);
    
    // 更新内部状态
    d->currentMinGrayValue = minGrayValue;
    d->currentMaxGrayValue = maxGrayValue;
    d->useGrayValueLimits = (minGrayValue < maxGrayValue);
    
    qDebug() << "API设置灰度值限制: [" << minGrayValue << ", " << maxGrayValue << "]";
    
    // 同时更新NiftiManager（如果已经有区块的话）
    if (d->niftiManager) {
        d->niftiManager->setGrayValueLimits(minGrayValue, maxGrayValue);
    }
}

// ========== 信息获取 ==========

QList<int> NiftiVisualizationAPI::getAllLabels() const
{
    Q_D(const NiftiVisualizationAPI);
    return d->niftiManager->getAllLabels();
}

QColor NiftiVisualizationAPI::getRegionColor(int label) const
{
    Q_D(const NiftiVisualizationAPI);
    BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
    return volume ? volume->getColor() : QColor();
}

bool NiftiVisualizationAPI::isRegionVisible(int label) const
{
    Q_D(const NiftiVisualizationAPI);
    BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
    return volume ? volume->isVisible() : false;
}

double NiftiVisualizationAPI::getRegionOpacity(int label) const
{
    Q_D(const NiftiVisualizationAPI);
    BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
    // 这里需要在BrainRegionVolume中添加getOpacity方法
    return volume ? 0.8 : 0.0; // 临时返回默认值
}

// ========== 状态查询 ==========

bool NiftiVisualizationAPI::hasMriData() const
{
    Q_D(const NiftiVisualizationAPI);
    return d->niftiManager->hasMriData();
}

bool NiftiVisualizationAPI::hasLabelData() const
{
    Q_D(const NiftiVisualizationAPI);
    return d->niftiManager->hasLabelData();
}

int NiftiVisualizationAPI::getRegionCount() const
{
    Q_D(const NiftiVisualizationAPI);
    return d->niftiManager->getAllLabels().size();
}

bool NiftiVisualizationAPI::hasProcessedRegions() const
{
    Q_D(const NiftiVisualizationAPI);
    return !d->niftiManager->getAllLabels().isEmpty();
}

// ========== 回调设置 ==========

void NiftiVisualizationAPI::setErrorCallback(std::function<void(const QString&)> callback)
{
    Q_D(NiftiVisualizationAPI);
    d->errorCallback = callback;
}

void NiftiVisualizationAPI::setRegionsProcessedCallback(std::function<void()> callback)
{
    Q_D(NiftiVisualizationAPI);
    d->regionsProcessedCallback = callback;
}

void NiftiVisualizationAPI::setRegionVisibilityCallback(std::function<void(int, bool)> callback)
{
    Q_D(NiftiVisualizationAPI);
    d->regionVisibilityCallback = callback;
}

// ========== 高级功能 ==========

void NiftiVisualizationAPI::resetCamera()
{
    Q_D(NiftiVisualizationAPI);
    if (d->renderer) {
        d->renderer->ResetCamera();
    }
}

void NiftiVisualizationAPI::render()
{
    Q_D(NiftiVisualizationAPI);
    if (d->renderer && d->renderer->GetRenderWindow()) {
        d->renderer->GetRenderWindow()->Render();
    }
}

bool NiftiVisualizationAPI::exportRegionInfo(const QString& filePath) const
{
    Q_D(const NiftiVisualizationAPI);
    
    // 简单的导出实现
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    out << "Region Information Export\n";
    out << "========================\n\n";
    
    QList<int> labels = d->niftiManager->getAllLabels();
    for (int label : labels) {
        BrainRegionVolume* volume = d->niftiManager->getRegionVolume(label);
        if (volume) {
            out << "Region " << label << ":\n";
            out << "  Color: " << volume->getColor().name() << "\n";
            out << "  Visible: " << (volume->isVisible() ? "Yes" : "No") << "\n";
            out << "  Centroid: " << volume->getCentroid().x() << ", " 
                << volume->getCentroid().y() << ", " << volume->getCentroid().z() << "\n\n";
        }
    }
    
    return true;
} 