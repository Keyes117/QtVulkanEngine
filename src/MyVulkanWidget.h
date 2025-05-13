#pragma once

#include <QMainWindow>
#include "ui_MyVulkanWidget.h"
#include "MyVulkanWindow.h"

#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MyVulkanWidgetClass; };
QT_END_NAMESPACE


class MyVulkanWidget : public QMainWindow
{
    Q_OBJECT

public:
    MyVulkanWidget(QWidget* parent = nullptr);
    ~MyVulkanWidget();

    MyVulkanWindow& getVulkanWindowHandle() { return m_vulkanWindow; }

protected:
    void keyPressEvent(QKeyEvent* event) override;

    //slots
private:
    void createToolBar();
    void createProjectPanel();
    void createVulkanContainer();

private:
    std::unique_ptr<Ui::MyVulkanWidgetClass> m_ui;


    MyVulkanWindow m_vulkanWindow;
};
