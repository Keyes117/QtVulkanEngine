#include "MyVulkanWindow.h"

#include <stdexcept>


MyVulkanWindow::MyVulkanWindow(QWindow* parent)
    :QWindow(parent)
{

}

MyVulkanWindow::~MyVulkanWindow()
{
}

void MyVulkanWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = ::GetModuleHandle(nullptr);
    createInfo.hwnd = reinterpret_cast<HWND>(winId());

    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to crate Win32 Vulkan Surface");
    }
}

void MyVulkanWindow::mousePressEvent(QMouseEvent* event)
{
    // 获取窗口的尺寸
    int windowWidth = this->width();
    int windowHeight = this->height();

    // 获取鼠标位置
    QPoint mousePos = event->pos();

    // 将屏幕坐标转换为归一化设备坐标 [-1, 1]
    float normalizedX = (2.0f * mousePos.x()) / windowWidth - 1.0f;
    float normalizedY = 1.0f - (2.0f * mousePos.y()) / windowHeight;

    emit mousePressed(event);

    QWindow::mousePressEvent(event);

}

void MyVulkanWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);

    m_framebufferResized = true;
    m_width = event->size().width();
    m_width = event->size().height();

}

void MyVulkanWindow::exposeEvent(QExposeEvent* event)
{
    if (isExposed())
        requestUpdate();
}

bool MyVulkanWindow::event(QEvent* event)
{
    if (event->type() == QEvent::UpdateRequest && isExposed())
    {
        emit updateRequested();
        return true;
    }
    return QWindow::event(event);
}

void MyVulkanWindow::keyPressEvent(QKeyEvent* event)
{
    emit keyPressed(event->key(), event->modifiers());

    QWindow::keyPressEvent(event);
}
