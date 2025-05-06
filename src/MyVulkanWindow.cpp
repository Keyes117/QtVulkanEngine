#include "MyVulkanWindow.h"

#include <stdexcept>


MyVulkanWindow::MyVulkanWindow(QWindow* parent)
    :QWindow(parent)
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

void MyVulkanWindow::resizeEvent(QResizeEvent* event)
{
    QWindow::resizeEvent(event);

    m_framebufferResized = true;
    m_width = event->size().width();
    m_width = event->size().height();

}

