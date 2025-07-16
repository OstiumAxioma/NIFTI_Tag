#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// VTK模块初始化 - 解决 "no override found for 'vtkRenderer'" 错误
#include <vtkAutoInit.h>
VTK_MODULE_INIT(vtkRenderingOpenGL2)
VTK_MODULE_INIT(vtkInteractionStyle)

#include <QMainWindow>
#include <QVTKOpenGLWidget.h>
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QMenuBar;
class QStatusBar;
class QToolBar;
class QDockWidget;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QSlider;
class QSpinBox;
class QGroupBox;
class QCheckBox;
QT_END_NAMESPACE

// 前向声明 - 只能使用API接口
class NiftiVisualizationAPI;

/**
 * @brief MainWindow - NIFTI可视化API使用示例
 * 
 * 重要：这个类仅作为API使用示例，不包含任何核心功能实现！
 * 所有NIFTI处理功能都通过NiftiVisualizationAPI接口调用。
 * 
 * 示例功能：
 * - 展示如何创建和配置API实例
 * - 展示如何处理文件加载和错误
 * - 展示如何实现UI控制和状态同步
 * - 展示如何集成到VTK渲染管线
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void setupSimpleWidget();
    void setupVTKWidget();
    
    // NIFTI文件导入槽函数（通过API）
    void importMriNiftiFile();
    void importLabelNiftiFile();
    void processNiftiRegions();
    
    // 区块管理槽函数（通过API）
    void updateUiForRegions();
    void onRegionSelectionChanged(QListWidgetItem* item);
    void sortVolumesByCameraDistance();
    void showAllRegions();
    void hideAllRegions();
    
    // 灰度值控制槽函数
    void onGrayValueChanged();
    void onPreviewButtonClicked();
    void onMriPreviewToggled(bool checked);
    
    // API回调响应
    void onNiftiError(const QString& message);
    void onRegionsProcessed();
    void onRegionVisibilityChanged(int label, bool visible);

private:
    // UI组件
    QVTKOpenGLWidget *vtkWidget;
    QMenu *fileMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QAction *exitAct;
    QAction *aboutAct;
    
    // NIFTI相关动作
    QAction *importMriAct;
    QAction *importLabelAct;
    QAction *processRegionsAct;
    QAction *testVolumeAct;
    
    // 区块控制面板
    QDockWidget *regionDockWidget;
    QListWidget *regionListWidget;
    QPushButton *showAllButton;
    QPushButton *hideAllButton;
    QPushButton *sortVolumesButton;
    QLabel *statusLabel;
    
    // 灰度值控制
    QGroupBox *grayValueGroupBox;
    QSlider *minGraySlider;
    QSlider *maxGraySlider;
    QSpinBox *minGraySpinBox;
    QSpinBox *maxGraySpinBox;
    QPushButton *previewButton;
    QCheckBox *mriPreviewCheckBox;

    // VTK组件
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderWindow> renderWindow;
    vtkSmartPointer<vtkRenderWindowInteractor> renderWindowInteractor;
    
    // 静态库API接口 - 唯一允许的功能访问方式
    NiftiVisualizationAPI *niftiAPI;
    
    // 私有方法
    void setupRegionControlPanel();
    void updateRegionList();
    void updateActionStates();
    void setupAPICallbacks();
};

#endif // MAINWINDOW_H 