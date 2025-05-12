#pragma once

#include "const.h"
#include "Device.h"
#include "Model.h"
#include "MyVulkanWidget.h"
#include "MyVulkanWindow.h"
#include "Object.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "RenderSystem.h"
#include "SwapChain.h"

#include <memory>
#include <vector>


class MyVulkanApp
{
public:
    MyVulkanApp();
    ~MyVulkanApp();


    MyVulkanApp(const MyVulkanApp&) = delete;
    MyVulkanApp& operator=(const MyVulkanApp&) = delete;

    MyVulkanApp(MyVulkanApp&&) = delete;
    MyVulkanApp& operator=(MyVulkanApp&&) = delete;

    void run();

    RenderSystem* getRenderSystem() { return &m_renderSystem; }
private:
    void loadObjects();

private:


    MyVulkanWidget                  m_widget;               //�����࣬�������Ǹ�windowһ���ĳ�����
    MyVulkanWindow& m_window;                               //���ƴ����е�vulkan ����

    Camera                          m_camera{};               //�����
    Device                          m_device;               //�����ʼ��Vulkan�������
    Renderer                        m_renderer;             //swapchain �� commandbuffer
    RenderSystem                    m_renderSystem;         //pipeline �� pipelineLayout
    std::vector<Object>             m_objects;              // �滭����


};

