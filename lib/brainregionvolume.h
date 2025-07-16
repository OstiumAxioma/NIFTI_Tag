#ifndef BRAINREGIONVOLUME_H
#define BRAINREGIONVOLUME_H

#include <QObject>
#include <QColor>
#include <QVector3D>

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkImageData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkCamera.h>

class BrainRegionVolume : public QObject
{
    Q_OBJECT

public:
    explicit BrainRegionVolume(int label, QObject *parent = nullptr);
    ~BrainRegionVolume();

    // 基本属性
    int getLabel() const { return label; }
    QColor getColor() const { return color; }
    bool isVisible() const { return visible; }
    QVector3D getCentroid() const { return centroid; }

    // VTK对象获取
    vtkActor* getSurfaceActor() const { return surfaceActor; }
    vtkActor* getCentroidSphere() const { return centroidSphere; }

    // 数据设置
    void setVolumeData(vtkImageData* mriData, vtkImageData* maskData);
    void setVolumeData(vtkImageData* mriData, vtkImageData* maskData, double minGrayValue, double maxGrayValue);
    void calculateCentroid();

    // 显示控制
    void updateVisibility(bool visible);
    void updateColor(const QColor& color);

    // 渲染排序
    double distanceToCamera(vtkCamera* camera) const;

    // 体绘制参数
    void setOpacity(double opacity);
    void setSampleDistance(double distance);
    
    // 灰度值限制参数
    void setGrayValueLimits(double minGrayValue, double maxGrayValue);

signals:
    void visibilityChanged(int label, bool visible);
    void colorChanged(int label, const QColor& color);

private:
    // 基本属性
    int label;
    QColor color;
    bool visible;
    QVector3D centroid;

    // VTK对象
    vtkSmartPointer<vtkActor> surfaceActor;
    vtkSmartPointer<vtkPolyDataMapper> surfaceMapper;
    vtkSmartPointer<vtkActor> centroidSphere;
    
    // 灰度值限制参数
    double minGrayValue;
    double maxGrayValue;
    bool useGrayValueLimits;

    // 私有方法
    void initializeSurfaceActor();
    void initializeCentroidSphere();
    void setupSurfaceProperty();
    void updateSurfaceColor();
    void updateSurfaceOpacity();
};

#endif // BRAINREGIONVOLUME_H 