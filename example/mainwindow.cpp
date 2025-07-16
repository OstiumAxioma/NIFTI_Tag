#include "mainwindow.h"
#include "../api/NiftiVisualizationAPI.h"  // 唯一允许的头文件

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QVTKOpenGLWidget.h>
#include <QDockWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSplitter>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QCheckBox>

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkCamera.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , niftiAPI(nullptr)
{
    setWindowTitle("NIFTI脑影像可视化 - API使用示例");
    resize(1200, 800);

    // 创建NIFTI可视化API实例
    niftiAPI = new NiftiVisualizationAPI(this);
    
    // 设置API回调
    setupAPICallbacks();

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    setupRegionControlPanel();
    
    // 先创建一个简单的界面，确保程序能稳定运行
    setupSimpleWidget();
    
    // 延迟初始化VTK组件
    QTimer::singleShot(2000, this, &MainWindow::setupVTKWidget);
    
    // 初始化动作状态
    updateActionStates();
}

MainWindow::~MainWindow()
{
    // niftiAPI会被自动删除（parent机制）
}

void MainWindow::setupAPICallbacks()
{
    // 设置API回调函数
    niftiAPI->setErrorCallback([this](const QString& message) {
        onNiftiError(message);
    });
    
    niftiAPI->setRegionsProcessedCallback([this]() {
        onRegionsProcessed();
    });
    
    niftiAPI->setRegionVisibilityCallback([this](int label, bool visible) {
        onRegionVisibilityChanged(label, visible);
    });
}

void MainWindow::createActions()
{
    // 退出动作
    exitAct = new QAction("退出(&Q)", this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip("退出应用程序");
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // 关于动作
    aboutAct = new QAction("关于(&A)", this);
    aboutAct->setStatusTip("显示应用程序的关于对话框");
    connect(aboutAct, &QAction::triggered, [this]() {
        QMessageBox::about(this, "关于 NIFTI脑影像可视化API示例",
                          "这是NIFTI脑影像可视化静态库的使用示例。\n\n"
                          "功能特性：\n"
                          "• 通过API接口加载MRI和标签NIFTI文件\n"
                          "• 通过API接口进行脑区块可视化\n"
                          "• 通过API接口控制区块显示/隐藏\n"
                          "• 展示静态库的正确使用方法\n\n"
                          "重要：本程序仅为API使用示例，不包含任何核心功能实现！");
    });

    // NIFTI文件导入动作
    importMriAct = new QAction("导入MRI文件(&M)", this);
    importMriAct->setStatusTip("通过API导入MRI NIFTI文件");
    connect(importMriAct, &QAction::triggered, this, &MainWindow::importMriNiftiFile);

    importLabelAct = new QAction("导入标签文件(&L)", this);
    importLabelAct->setStatusTip("通过API导入脑区标签NIFTI文件");
    connect(importLabelAct, &QAction::triggered, this, &MainWindow::importLabelNiftiFile);

    processRegionsAct = new QAction("处理区块(&P)", this);
    processRegionsAct->setStatusTip("通过API处理脑区块并生成可视化");
    connect(processRegionsAct, &QAction::triggered, this, &MainWindow::processNiftiRegions);
    
    // 添加测试体绘制动作
    testVolumeAct = new QAction("测试体绘制(&T)", this);
    testVolumeAct->setStatusTip("测试基础NIFTI体绘制功能");
    connect(testVolumeAct, &QAction::triggered, [this]() {
        statusBar()->showMessage("正在测试体绘制...");
        niftiAPI->testSimpleVolumeRendering();
        statusBar()->showMessage("体绘制测试完成", 3000);
    });
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(importMriAct);
    fileMenu->addAction(importLabelAct);
    fileMenu->addSeparator();
    fileMenu->addAction(processRegionsAct);
    fileMenu->addAction(testVolumeAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar("文件");
    fileToolBar->addAction(importMriAct);
    fileToolBar->addAction(importLabelAct);
    fileToolBar->addSeparator();
    fileToolBar->addAction(processRegionsAct);
    fileToolBar->addAction(testVolumeAct);
    fileToolBar->addSeparator();
    fileToolBar->addAction(exitAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("API示例就绪 - 请先导入MRI和标签NIFTI文件");
}

void MainWindow::setupSimpleWidget()
{
    QLabel *label = new QLabel("NIFTI脑影像可视化API示例正在初始化...", this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("QLabel { font-size: 18px; color: blue; }");
    setCentralWidget(label);
    statusBar()->showMessage("简单界面创建成功", 2000);
}

void MainWindow::setupVTKWidget()
{
    try {
        statusBar()->showMessage("正在初始化VTK...", 1000);

        // 步骤1：创建QVTKOpenGLWidget
        vtkWidget = new QVTKOpenGLWidget(this);
        setCentralWidget(vtkWidget);

        // 步骤2：创建VTK对象
        renderer = vtkSmartPointer<vtkRenderer>::New();
        renderer->SetBackground(0.1, 0.2, 0.4); // 深蓝色背景

        // 步骤3：获取QVTKOpenGLWidget的渲染窗口
        renderWindow = vtkWidget->GetRenderWindow();
        renderWindow->AddRenderer(renderer);

        // 步骤4：设置交互器
        renderWindowInteractor = renderWindow->GetInteractor();
        renderWindowInteractor->SetRenderWindow(renderWindow);
        renderWindowInteractor->Initialize();

        // 步骤5：将渲染器设置到API
        niftiAPI->setRenderer(renderer);

        statusBar()->showMessage("VTK集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("VTK初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("VTK初始化失败", 3000);
    }
}

void MainWindow::setupRegionControlPanel()
{
    // 创建停靠窗口
    regionDockWidget = new QDockWidget("脑区控制面板 (API示例)", this);
    regionDockWidget->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    // 创建主控件
    QWidget *dockWidgetContents = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(dockWidgetContents);
    
    // 状态标签
    statusLabel = new QLabel("未加载数据");
    statusLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");
    layout->addWidget(statusLabel);
    
    // 灰度值控制组
    grayValueGroupBox = new QGroupBox("MRI灰度值限制");
    QVBoxLayout *grayLayout = new QVBoxLayout(grayValueGroupBox);
    
    // 最小灰度值控制
    QHBoxLayout *minGrayLayout = new QHBoxLayout();
    minGrayLayout->addWidget(new QLabel("最小值:"));
    minGraySlider = new QSlider(Qt::Horizontal);
    minGraySlider->setRange(0, 10000);
    minGraySlider->setValue(0);
    minGraySpinBox = new QSpinBox();
    minGraySpinBox->setRange(0, 10000);
    minGraySpinBox->setValue(0);
    minGrayLayout->addWidget(minGraySlider);
    minGrayLayout->addWidget(minGraySpinBox);
    grayLayout->addLayout(minGrayLayout);
    
    // 最大灰度值控制
    QHBoxLayout *maxGrayLayout = new QHBoxLayout();
    maxGrayLayout->addWidget(new QLabel("最大值:"));
    maxGraySlider = new QSlider(Qt::Horizontal);
    maxGraySlider->setRange(0, 10000);
    maxGraySlider->setValue(3000);
    maxGraySpinBox = new QSpinBox();
    maxGraySpinBox->setRange(0, 10000);
    maxGraySpinBox->setValue(3000);
    maxGrayLayout->addWidget(maxGraySlider);
    maxGrayLayout->addWidget(maxGraySpinBox);
    grayLayout->addLayout(maxGrayLayout);
    
    // 预览按钮和显示开关
    QHBoxLayout *previewLayout = new QHBoxLayout();
    previewButton = new QPushButton("预览MRI");
    previewButton->setEnabled(false);
    previewLayout->addWidget(previewButton);
    
    mriPreviewCheckBox = new QCheckBox("显示MRI预览");
    mriPreviewCheckBox->setEnabled(false);
    mriPreviewCheckBox->setChecked(true);
    previewLayout->addWidget(mriPreviewCheckBox);
    
    grayLayout->addLayout(previewLayout);
    
    // 连接信号
    connect(minGraySlider, &QSlider::valueChanged, minGraySpinBox, &QSpinBox::setValue);
    connect(minGraySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), minGraySlider, &QSlider::setValue);
    connect(maxGraySlider, &QSlider::valueChanged, maxGraySpinBox, &QSpinBox::setValue);
    connect(maxGraySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), maxGraySlider, &QSlider::setValue);
    
    connect(minGraySlider, &QSlider::valueChanged, this, &MainWindow::onGrayValueChanged);
    connect(maxGraySlider, &QSlider::valueChanged, this, &MainWindow::onGrayValueChanged);
    connect(previewButton, &QPushButton::clicked, this, &MainWindow::onPreviewButtonClicked);
    connect(mriPreviewCheckBox, &QCheckBox::toggled, this, &MainWindow::onMriPreviewToggled);
    
    layout->addWidget(grayValueGroupBox);
    
    // 区块列表
    regionListWidget = new QListWidget();
    regionListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    connect(regionListWidget, &QListWidget::itemChanged,
            this, &MainWindow::onRegionSelectionChanged);
    layout->addWidget(regionListWidget);
    
    // 按钮组
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    showAllButton = new QPushButton("显示全部");
    connect(showAllButton, &QPushButton::clicked, this, &MainWindow::showAllRegions);
    buttonLayout->addWidget(showAllButton);
    
    hideAllButton = new QPushButton("隐藏全部");
    connect(hideAllButton, &QPushButton::clicked, this, &MainWindow::hideAllRegions);
    buttonLayout->addWidget(hideAllButton);
    
    layout->addLayout(buttonLayout);
    
    sortVolumesButton = new QPushButton("排序Volume");
    connect(sortVolumesButton, &QPushButton::clicked, 
            this, &MainWindow::sortVolumesByCameraDistance);
    layout->addWidget(sortVolumesButton);
    
    regionDockWidget->setWidget(dockWidgetContents);
    addDockWidget(Qt::RightDockWidgetArea, regionDockWidget);
}

void MainWindow::importMriNiftiFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "导入MRI NIFTI文件", "", "NIFTI文件 (*.nii *.nii.gz)");
    
    if (!fileName.isEmpty()) {
        statusBar()->showMessage("正在通过API加载MRI文件...");
        if (niftiAPI->loadMriNifti(fileName)) {
            statusBar()->showMessage("MRI文件加载成功", 3000);
            updateActionStates();
        } else {
            statusBar()->showMessage("MRI文件加载失败", 3000);
        }
    }
}

void MainWindow::importLabelNiftiFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "导入标签NIFTI文件", "", "NIFTI文件 (*.nii *.nii.gz)");
    
    if (!fileName.isEmpty()) {
        statusBar()->showMessage("正在通过API加载标签文件...");
        if (niftiAPI->loadLabelNifti(fileName)) {
            statusBar()->showMessage("标签文件加载成功", 3000);
            updateActionStates();
        } else {
            statusBar()->showMessage("标签文件加载失败", 3000);
        }
    }
}

void MainWindow::processNiftiRegions()
{
    if (!niftiAPI->hasMriData() || !niftiAPI->hasLabelData()) {
        QMessageBox::warning(this, "警告", "需要同时加载MRI和标签数据才能处理区块");
        return;
    }
    
    statusBar()->showMessage("正在通过API处理脑区块...");
    niftiAPI->processRegions();
}

void MainWindow::updateUiForRegions()
{
    updateRegionList();
    updateActionStates();
}

void MainWindow::onRegionSelectionChanged(QListWidgetItem* item)
{
    if (!item) return;
    
    int label = item->data(Qt::UserRole).toInt();
    bool visible = (item->checkState() == Qt::Checked);
    
    // 通过API设置区块可见性
    niftiAPI->setRegionVisibility(label, visible);
    
    // 刷新渲染
    niftiAPI->render();
}

void MainWindow::sortVolumesByCameraDistance()
{
    niftiAPI->sortVolumesByCamera();
    niftiAPI->render();
    statusBar()->showMessage("Volume排序完成", 2000);
}

void MainWindow::showAllRegions()
{
    niftiAPI->setAllRegionsVisibility(true);
    updateRegionList();
    niftiAPI->render();
}

void MainWindow::hideAllRegions()
{
    niftiAPI->setAllRegionsVisibility(false);
    updateRegionList();
    niftiAPI->render();
}

void MainWindow::onGrayValueChanged()
{
    // 实时更新API中的灰度值限制
    double minValue = minGraySpinBox->value();
    double maxValue = maxGraySpinBox->value();
    
    // 确保最小值不大于最大值
    if (minValue >= maxValue) {
        if (sender() == minGraySlider || sender() == minGraySpinBox) {
            // 如果是最小值改变，调整最大值
            maxValue = minValue + 1;
            maxGraySlider->setValue(maxValue);
            maxGraySpinBox->setValue(maxValue);
        } else {
            // 如果是最大值改变，调整最小值
            minValue = maxValue - 1;
            minGraySlider->setValue(minValue);
            minGraySpinBox->setValue(minValue);
        }
    }
    
    // 更新API中的灰度值限制
    niftiAPI->setGrayValueLimits(minValue, maxValue);
    
    statusBar()->showMessage(QString("灰度值限制: [%1, %2]").arg(minValue).arg(maxValue), 2000);
}

void MainWindow::onPreviewButtonClicked()
{
    if (!niftiAPI) return;
    
    statusBar()->showMessage("正在预览MRI...");
    
    // 调用API的预览方法
    niftiAPI->previewMriVisualization();
    
    // 预览完成后启用复选框
    mriPreviewCheckBox->setEnabled(true);
    
    statusBar()->showMessage("MRI预览完成", 3000);
}

void MainWindow::onMriPreviewToggled(bool checked)
{
    if (!niftiAPI) return;
    
    // 设置MRI预览的可见性
    niftiAPI->setMriPreviewVisible(checked);
    
    statusBar()->showMessage(checked ? "MRI预览已显示" : "MRI预览已隐藏", 2000);
}

void MainWindow::onNiftiError(const QString& message)
{
    QMessageBox::critical(this, "NIFTI错误", message);
    statusBar()->showMessage("错误: " + message, 5000);
}

void MainWindow::onRegionsProcessed()
{
    statusBar()->showMessage("脑区块处理完成", 3000);
    updateUiForRegions();
    
    // 重置相机视角
    niftiAPI->resetCamera();
    niftiAPI->render();
}

void MainWindow::onRegionVisibilityChanged(int label, bool visible)
{
    updateRegionList();
    statusBar()->showMessage(QString("区块 %1 %2").arg(label).arg(visible ? "显示" : "隐藏"), 2000);
}

void MainWindow::updateRegionList()
{
    regionListWidget->clear();
    
    QList<int> labels = niftiAPI->getAllLabels();
    if (labels.isEmpty()) {
        statusLabel->setText("未加载数据");
        return;
    }
    
    statusLabel->setText(QString("共 %1 个脑区块").arg(labels.size()));
    
    for (int label : labels) {
        QListWidgetItem* item = new QListWidgetItem(
            QString("区块 %1").arg(label));
        item->setData(Qt::UserRole, label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(niftiAPI->isRegionVisible(label) ? Qt::Checked : Qt::Unchecked);
        
        // 设置颜色显示
        QColor color = niftiAPI->getRegionColor(label);
        item->setBackground(QBrush(color.lighter(180)));
        item->setForeground(QBrush(color.darker(200)));
        
        regionListWidget->addItem(item);
    }
}

void MainWindow::updateActionStates()
{
    bool hasMri = niftiAPI->hasMriData();
    bool hasLabel = niftiAPI->hasLabelData();
    bool hasRegions = niftiAPI->hasProcessedRegions();
    
    processRegionsAct->setEnabled(hasMri && hasLabel);
    
    showAllButton->setEnabled(hasRegions);
    hideAllButton->setEnabled(hasRegions);
    sortVolumesButton->setEnabled(hasRegions);
    
    // 灰度值控制
    grayValueGroupBox->setEnabled(hasMri);
    previewButton->setEnabled(hasMri);
    
    // 复选框只有在预览完成后才启用，这里重置为禁用状态
    if (!hasMri) {
        mriPreviewCheckBox->setEnabled(false);
        mriPreviewCheckBox->setChecked(true);
    }
} 