#pragma once

#include "Device.h"
#include "MyVulkanWindow.h"
#include "Pipeline.h"
#include "SwapChain.h"
#include "Object.h"
#include "Model.h"
#include "const.h"

#include <memory>
#include <vector>


#include <QVector2D>
#include <QVector3D>

struct SimplePushConstantData
{
    Mat2F   transform;
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
    void loadObjects();
    void createPipelineLayout();
    void createPipeline();
    void createCommandBuffers();
    void freeCommandBuffers();
    void drawFrame();
    void renderObjects(VkCommandBuffer commandBuffer);

    void recreateSwapChain();
    void recordCommandBuffer(int imageIndex);
private:

    bool                            m_windowClosed{ false };

    MyVulkanWindow                  m_window;
    Device                          m_device;
    std::unique_ptr<SwapChain>      m_swapChain;
    std::unique_ptr<Pipeline>       m_pipeline;
    //std::unique_ptr<Model>          m_model;
    std::vector<Object>             m_objects;
    VkPipelineLayout                m_pipelineLayout;
    std::vector<VkCommandBuffer>    m_commandBuffers;

};

