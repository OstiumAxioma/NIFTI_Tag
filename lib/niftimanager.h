#ifndef NIFTIMANAGER_H
#define NIFTIMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QString>
#include <QColor>

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>

// 前向声明
class BrainRegionVolume;

class NiftiManager : public QObject
{
    Q_OBJECT

public:
    explicit NiftiManager(QObject *parent = nullptr);
    ~NiftiManager();

    // NIFTI文件读取
    bool loadMriNifti(const QString& filePath);
    bool loadLabelNifti(const QString& filePath);

    // 数据处理与分区
    void processRegions();
    void clearRegions();

    // 区块管理
    void updateRegionVisibility(int label, bool visible);
    void sortVolumesByCamera(vtkCamera* camera);
    
    // 获取信息
    QList<int> getAllLabels() const;
    BrainRegionVolume* getRegionVolume(int label);
    bool hasMriData() const { return mriImage != nullptr; }
    bool hasLabelData() const { return labelImage != nullptr; }

    // 渲染器设置
    void setRenderer(vtkRenderer* renderer);
    vtkRenderer* getRenderer() const { return renderer; }

signals:
    void regionsProcessed();
    void regionVisibilityChanged(int label, bool visible);
    void errorOccurred(const QString& message);

private:
    // 数据成员
    vtkSmartPointer<vtkImageData> mriImage;
    vtkSmartPointer<vtkImageData> labelImage;
    QMap<int, BrainRegionVolume*> regionVolumes;
    vtkRenderer* renderer;

    // 私有方法
    QList<int> extractLabelsFromImage();
    QColor generateColorForLabel(int label);
    void addVolumeToRenderer(BrainRegionVolume* volume);
    void removeVolumeFromRenderer(BrainRegionVolume* volume);
};

#endif // NIFTIMANAGER_H 