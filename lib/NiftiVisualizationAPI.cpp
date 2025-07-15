#include "../api/NiftiVisualizationAPI.h"
#include "niftimanager.h"
#include "brainregionvolume.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkCamera.h>

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
    
    // 暂时不添加Volume到渲染器，避免VTK崩溃
    qDebug() << "区块处理完成，暂时跳过渲染器添加";
    
    /* 暂时注释掉可能导致崩溃的代码
    // 处理完成后，确保所有Volume都已添加到渲染器
    if (d->renderer) {
        QList<int> labels = d->niftiManager->getAllLabels();
        for (int label : labels) {
            auto* volume = d->niftiManager->getRegionVolume(label);
            if (volume) {
                d->renderer->AddVolume(volume->getVolume());
                d->renderer->AddActor(volume->getCentroidSphere());
            }
        }
    }
    */
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