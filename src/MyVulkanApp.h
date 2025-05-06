#pragma once

#include "Device.h"
#include "MyVulkanWindow.h"
#include "Pipeline.h"
#include "SwapChain.h"

#include "Model.h"

#include <memory>
#include <vector>


struct SimplePushConstantData
{
    QVector2D offset;
    alignas(16) QVector3D color;
};

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

private:
    void loadModels();
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void freeCommandBuffers();
    void drawFrame();

    void recreateSwapChain();
    void recordCommandBuffer(int imageIndex);
private:
    MyVulkanWindow                  m_window;
    Device                          m_device;
    std::unique_ptr<SwapChain>      m_swapChain;
    std::unique_ptr<Pipeline>       m_pipeline;
    std::unique_ptr<Model>          m_model;

    VkPipelineLayout                m_pipelineLayout;
    std::vector<VkCommandBuffer>    m_commandBuffers;

};

