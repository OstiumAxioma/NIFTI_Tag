#include "mainwindow.h"
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

// VTK头文件
#include <vtkSmartPointer.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkSphereSource.h>
#include <vtkCamera.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("VTK Qt 项目");
    resize(800, 600);

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();
    
    // 先创建一个简单的界面，确保程序能稳定运行
    setupSimpleWidget();
    
    // 延迟初始化VTK组件
    QTimer::singleShot(2000, this, &MainWindow::setupVTKWidget);
}

MainWindow::~MainWindow()
{
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
        QMessageBox::about(this, "关于 VTK Qt 项目",
                          "这是一个基于VTK和Qt的3D可视化项目。");
    });

    // 创建球体动作
    sphereAct = new QAction("创建球体(&S)", this);
    sphereAct->setStatusTip("在场景中创建一个球体");
    connect(sphereAct, &QAction::triggered, this, &MainWindow::createSphere);
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction(exitAct);

    helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction(aboutAct);
}

void MainWindow::createToolBars()
{
    fileToolBar = addToolBar("文件");
    fileToolBar->addAction(sphereAct);
    fileToolBar->addAction(exitAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage("就绪");
}

void MainWindow::setupSimpleWidget()
{
    QLabel *label = new QLabel("VTK Qt 项目正在初始化...", this);
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

        statusBar()->showMessage("VTK集成到Qt界面成功！", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("VTK初始化失败: %1").arg(e.what()));
        statusBar()->showMessage("VTK初始化失败", 3000);
    }
}

void MainWindow::createSphere()
{
    try {
        // 创建球体源
        sphereSource = vtkSmartPointer<vtkSphereSource>::New();
        sphereSource->SetRadius(1.0);
        sphereSource->SetPhiResolution(20);
        sphereSource->SetThetaResolution(20);

        // 创建映射器
        mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        mapper->SetInputConnection(sphereSource->GetOutputPort());

        // 创建演员
        actor = vtkSmartPointer<vtkActor>::New();
        actor->SetMapper(mapper);

        // 添加演员到渲染器
        renderer->AddActor(actor);

        // 重置相机
        renderer->ResetCamera();

        // 刷新渲染窗口
        renderWindow->Render();

        statusBar()->showMessage("球体已创建", 2000);
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "警告", QString("创建球体失败: %1").arg(e.what()));
        statusBar()->showMessage("创建球体失败", 3000);
    }
} 