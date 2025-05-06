#pragma once

#include "ui_QtVulkan.h"
#include <QtWidgets/QWidget>

#include <pipeline.h>

#include <vulkan/vulkan.h>

QT_BEGIN_NAMESPACE
namespace Ui { class QtVulkanClass; };
QT_END_NAMESPACE

class QtVulkan : public QWidget
{
    Q_OBJECT

public:
    QtVulkan(QWidget* parent = nullptr);
    ~QtVulkan();

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

private:
    Ui::QtVulkanClass* m_ui;

};
