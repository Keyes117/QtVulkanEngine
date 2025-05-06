#include "QtVulkan.h"

QtVulkan::QtVulkan(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::QtVulkanClass())
{
    m_ui->setupUi(this);
}

QtVulkan::~QtVulkan()
{
    delete m_ui;
}

void QtVulkan::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{

}
