#pragma once

#include "const.h"
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

    RenderSystem* getRenderSystem() { return &m_renderSystem; }
private:
    void loadObjects();

private:

    std::chrono::high_resolution_clock::time_point          m_lastFrameTime;
    MyVulkanWidget                  m_widget;               //窗口类，后续考虑跟window一样改成引用
    MyVulkanWindow& m_window;                               //控制窗口中的vulkan 窗口
    Keyboard_Movement_Controller    m_keyBoardController;
    Mouse_Movement_Controller       m_mouseController;

    Object                          m_cameraObject;
    Camera                          m_camera;               //相机类
    Device                          m_device;               //负责初始化Vulkan基础组件
    Renderer                        m_renderer;             //swapchain 和 commandbuffer
    RenderSystem                    m_renderSystem;         //pipeline 和 pipelineLayout
    std::vector<Object>             m_objects;              // 绘画对象


};

