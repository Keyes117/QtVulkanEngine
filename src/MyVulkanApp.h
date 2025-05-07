#pragma once

#include "const.h"
#include "Device.h"
#include "Model.h"
#include "MyVulkanWindow.h"
#include "Object.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "SwapChain.h"
#include <memory>
#include <RenderSystem.h>
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

    MyVulkanWindow                  m_window;
    Device                          m_device;
    Renderer                        m_renderer;
    RenderSystem                    m_renderSystem;
    std::vector<Object>             m_objects;


};

