#include "MyVulkanWidget.h"


#include <QToolbar>
#include <QDockWidget>
#include <QVBoxLayout>

MyVulkanWidget::MyVulkanWidget(QWidget* parent)
    : QMainWindow(parent),
    m_ui(std::make_unique<Ui::MyVulkanWidgetClass>())
{
    m_ui->setupUi(this);
    createToolBar();
    createProjectPanel();
    createVulkanContainer();
}

MyVulkanWidget::~MyVulkanWidget()
{

}


void MyVulkanWidget::keyPressEvent(QKeyEvent* event)
{
    qDebug() << "widget catch " << event->key();
}

void MyVulkanWidget::createToolBar()
{
    QToolBar* toolBar = addToolBar("Main Tool Bar");

    //添加工具按钮
    QAction* actDrawNone = toolBar->addAction("Draw None");
    QAction* actDrawPoint = toolBar->addAction("Draw Point");
    QAction* actDrawLine = toolBar->addAction("Draw Line");
    QAction* actDrawPolygon = toolBar->addAction("Draw Polygon");

    connect(actDrawNone, &QAction::triggered, &m_vulkanWindow, &MyVulkanWindow::setDrawNone);
    connect(actDrawPoint, &QAction::triggered, &m_vulkanWindow, &MyVulkanWindow::setDrawPoint);
    connect(actDrawLine, &QAction::triggered, &m_vulkanWindow, &MyVulkanWindow::setDrawLine);
    connect(actDrawPolygon, &QAction::triggered, &m_vulkanWindow, &MyVulkanWindow::setDrawPolygon);
}

void MyVulkanWidget::createProjectPanel()
{
    QDockWidget* projectPanel = new QDockWidget("projectPanel", this);
    QWidget* projectWidget = new QWidget(projectPanel);
    QVBoxLayout* layout = new QVBoxLayout(projectWidget);

    projectWidget->setLayout(layout);
    projectPanel->setWidget(projectWidget);

    addDockWidget(Qt::LeftDockWidgetArea, projectPanel);
}

void MyVulkanWidget::createVulkanContainer()
{
    QWidget* vulkanWindow = QWidget::createWindowContainer(&m_vulkanWindow, this);
    vulkanWindow->setFocusPolicy(Qt::ClickFocus);
    setCentralWidget(vulkanWindow);

}
