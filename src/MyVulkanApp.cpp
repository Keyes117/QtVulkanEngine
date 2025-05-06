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

    loadModels();
    createPipelineLayout();
    createPipeline();
    createCommandBuffers();
    for (int i = 0; i < m_commandBuffers.size(); i++)
    {
        recordCommandBuffer(i);
    }
    drawFrame();

    QTimer* t = new QTimer(&m_window);
    t->setTimerType(Qt::PreciseTimer);
    QObject::connect(t, &QTimer::timeout, [this]() { drawFrame(); });
    t->start(16);
    return;
}

void MyVulkanApp::loadModels()
{
    std::vector<Model::Vertex> vertices
    {
        {{0.5f, -0.5f},{1.0f,0.0f,0.0f}},
        {{0.5f, 0.5f},{0.0f,1.0f,0.0f}},
        {{-0.5f,0.5f},{0.0f,0.0f,1.0f}}
    };

    m_model = std::make_unique<Model>(m_device, vertices);
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
    pipelineLayoutInfo.pushConstantRangeCount = 0;
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
        static_cast<float>(m_commandBuffers.size()),
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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result != VK_SUBOPTIMAL_KHR
        || m_window.isWidthResized())
    {
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
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

    // �þ� swapchain ������ʵ���Ը�����Դ
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
    static int frame = 0;
    frame = (frame + 1) % 1000;

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

    m_pipeline->bind(m_commandBuffers[imageIndex]);
    m_model->bind(m_commandBuffers[imageIndex]);

    for (int j = 0; j < 4; j++)
    {
        SimplePushConstantData push{};
        push.offset = { -0.5f + frame * 0.002f,-0.4f + j * 0.25f };
        push.color = { 0.0f, 0.0f, 0.2f + 0.2f * j };

        vkCmdPushConstants(
            m_commandBuffers[imageIndex],
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);

        m_model->draw(m_commandBuffers[imageIndex]);
    }



    vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
    if (vkEndCommandBuffer(m_commandBuffers[imageIndex]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}
