#pragma once

#include "const.h"
#include "Device.h"
#include "Model.h"
#include "MyVulkanWindow.h"
#include "MyVulkanWidget.h"
#include "Object.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "SwapChain.h"
#include "RenderSystem.h"

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
    //QTimer* getRenderTimer() { return m_renderTimer.get(); }
private:
    void loadObjects();

private:


    MyVulkanWidget                  m_widget;
    MyVulkanWindow& m_window;

    Device                          m_device;
    Renderer                        m_renderer;
    RenderSystem                    m_renderSystem;
    //std::unique_ptr<QTimer>         m_renderTimer;
    std::vector<Object>             m_objects;


};

