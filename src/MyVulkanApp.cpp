#include "MyVulkanApp.h"

#include <array>
#include <QApplication>
#include <qtimer.h>

MyVulkanApp::MyVulkanApp()
    :m_device(m_window)
{

}

MyVulkanApp::~MyVulkanApp()
{
    vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
}

void MyVulkanApp::run()
{
    m_window.resize(800, 600);

    m_window.show();

    m_swapChain = std::make_unique<SwapChain>(m_device, m_window.getExtent());

    loadObjects();
    createPipelineLayout();
    createPipeline();
    createCommandBuffers();

    drawFrame();

    QTimer* t = new QTimer(&m_window);
    t->setTimerType(Qt::PreciseTimer);

    QObject::connect(&m_window, &QWindow::close, [this]() {
        m_windowClosed = true;
        });

    QObject::connect(t, &QTimer::timeout, [this]() {
        if (m_windowClosed)
            return true;
        drawFrame();
        });
    QObject::connect(&m_window, &QWindow::close, t, [t]() {
        t->setInterval(0);  // 暂停定时器
        t->disconnect();
        });
    t->start(16);
    return;
}

void MyVulkanApp::loadObjects()
{
    std::vector<Model::Vertex> vertices
    {
        {{0.5f, -0.5f},{1.0f,0.0f,0.0f}},
        {{0.5f, 0.5f},{0.0f,1.0f,0.0f}},
        {{-0.5f,0.5f},{0.0f,0.0f,1.0f}}
    };

    auto model = std::make_shared<Model>(m_device, vertices);

    for (int i = 0; i < 10; i++)
    {
        auto triangle = Object::createObject();
        triangle.m_model = model;
        triangle.m_color = { .1f, .8f , .1f };
        triangle.m_transform2d.translation.setX(.2f);
        triangle.m_transform2d.scale = { .2f ,.5f };
        triangle.m_transform2d.rotation = .25f * M_PI;

        m_objects.push_back(std::move(triangle));
    }

}


void MyVulkanApp::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo,
        nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void MyVulkanApp::createPipeline()
{
    assert(m_swapChain != nullptr && "Cannot create pipeline before swap chain");
    assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfigInfo{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfigInfo);

    pipelineConfigInfo.renderPass = m_swapChain->getRenderPass();
    pipelineConfigInfo.pipelineLayout = m_pipelineLayout;
    m_pipeline = std::make_unique<Pipeline>(
        m_device,
        "simple_shader.vert.spv",
        "simple_shader.frag.spv",
        pipelineConfigInfo
    );


}

void MyVulkanApp::createCommandBuffers()
{
    m_commandBuffers.resize(m_swapChain->imageCount());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_device.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device.device(), &allocInfo,
        m_commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffer");
    }
}

void MyVulkanApp::freeCommandBuffers()
{
    vkFreeCommandBuffers(
        m_device.device(),
        m_device.getCommandPool(),
        static_cast<uint32_t>(m_commandBuffers.size()),
        m_commandBuffers.data()
    );
    m_commandBuffers.clear();
}

void MyVulkanApp::drawFrame()
{
    auto extent = m_window.getExtent();
    if (extent.width == 0 || extent.height == 0)
        return;


    uint32_t imageIndex;
    auto result = m_swapChain->acquireNextImage(&imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    recordCommandBuffer(imageIndex);
    result = m_swapChain->submitCommandBuffers(&m_commandBuffers[imageIndex], &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || m_window.isWidthResized())
    {
        if (m_windowClosed)
            return;
        recreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
}

void MyVulkanApp::renderObjects(VkCommandBuffer commandBuffer)
{
    int i = 0;
    for (auto& obj : m_objects)
    {
        i += 1;

        float angle = std::fmod(obj.m_transform2d.rotation + 0.01f * i, 2.f * M_PI);
        if (angle < 0) {
            angle += 2 * M_PI; // Ensure non-negative
        }
        obj.m_transform2d.rotation = angle;
    }

    m_pipeline->bind(commandBuffer);

    for (auto& obj : m_objects)
    {

        SimplePushConstantData push{};
        push.offset = obj.m_transform2d.translation;
        push.color = obj.m_color;
        push.transform = obj.m_transform2d.mat2f();
        vkCmdPushConstants(
            commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);
        obj.m_model->bind(commandBuffer);
        obj.m_model->draw(commandBuffer);
    }
}

void MyVulkanApp::recreateSwapChain()
{
    vkDeviceWaitIdle(m_device.device());

    auto extent = m_window.getExtent();
    while (extent.width == 0 || extent.height == 0) {
        QCoreApplication::processEvents();
        extent = m_window.getExtent();
    }

    // 用旧 swapchain 创建新实例以复用资源
    if (m_swapChain == nullptr)
    {
        m_swapChain = std::make_unique<SwapChain>(
            m_device,
            extent
        );
    }
    else
    {
        m_swapChain = std::make_unique<SwapChain>(
            m_device,
            extent,
            std::move(m_swapChain)
        );
        if (m_swapChain->imageCount() != m_commandBuffers.size())
        {
            freeCommandBuffers();
            createCommandBuffers();
        }
    }

    //loadModels();
    //createPipelineLayout();
    //if render pass compatable do nothing else
    createPipeline();
    //createCommandBuffers();
}

void MyVulkanApp::recordCommandBuffer(int imageIndex)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapChain->getRenderPass();
    renderPassInfo.framebuffer = m_swapChain->getFrameBuffer(imageIndex);

    renderPassInfo.renderArea.offset = { 0,0 };
    renderPassInfo.renderArea.extent = m_swapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { 0.01f, 0.01f , 0.01f , 0.1f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(m_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(m_swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D sicssor{ {0,0}, m_swapChain->getSwapChainExtent() };
    vkCmdSetViewport(m_commandBuffers[imageIndex], 0, 1, &viewport);
    vkCmdSetScissor(m_commandBuffers[imageIndex], 0, 1, &sicssor);

    renderObjects(m_commandBuffers[imageIndex]);


    vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
    if (vkEndCommandBuffer(m_commandBuffers[imageIndex]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}
