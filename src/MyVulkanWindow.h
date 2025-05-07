#pragma once

#include <qevent.h>
#include <QWindow>

#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

class MyVulkanWindow : public QWindow
{
    Q_OBJECT
public:
    explicit MyVulkanWindow(QWindow* parent = nullptr);
    ~MyVulkanWindow() = default;

    void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

    VkExtent2D getExtent() {
        return { static_cast<uint32_t>(m_width),static_cast<uint32_t>(m_height) };
    }

    bool isWidthResized() { return m_framebufferResized; }
    void resetWindowResizedFlag() { m_framebufferResized = false; }



    int width() { return m_width; }
    int height() { return m_height; }
protected:
    void resizeEvent(QResizeEvent* event) override;


private:
    int m_width{ 800 };
    int m_height{ 600 };

    bool m_framebufferResized{ false };


};

