#pragma once

#include "const.h"
#include "Descriptors.h"
#include "Device.h"
#include "Model.h"
#include "Movement_Controller.h"
#include "MyVulkanWidget.h"
#include "MyVulkanWindow.h"
#include "Object.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "RenderSystem.h"
#include "SwapChain.h"


#include <chrono>
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

    std::shared_ptr<RenderSystem> getRenderSystem() { return m_renderSystem; }
private:
    void loadObjects();

private:

    std::chrono::high_resolution_clock::time_point          m_lastFrameTime;
    MyVulkanWidget                                          m_widget;               //�����࣬�������Ǹ�windowһ���ĳ�����
    MyVulkanWindow& m_window;                                                       //���ƴ����е�vulkan ����
    Keyboard_Movement_Controller                            m_keyBoardController;
    Mouse_Movement_Controller                               m_mouseController;

    Object                                                  m_cameraObject;
    Camera                                                  m_camera;               //�����
    Device                                                  m_device;               //�����ʼ��Vulkan�������
    Renderer                                                m_renderer;             //swapchain �� commandbuffer
    std::shared_ptr<RenderSystem>                           m_renderSystem;         //pipeline �� pipelineLayout
    std::vector<Object>                                     m_objects;              // �滭����

    std::unique_ptr<DescriptorPool>                         m_globalPool{};
    std::unique_ptr<DescriptorSetLayout>                    m_globalSetLayout;
    std::vector<VkDescriptorSet>                            m_globalDescriptSets;
    std::vector<std::unique_ptr<Buffer>>                    m_spGlobalUboBuffers;
};

