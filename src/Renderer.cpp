#include "Renderer.h"

#include <array>
#include <QApplication>
#include <qtimer.h>


Renderer::Renderer(MyVulkanWindow& window, Device& device) :
    m_window(window),
    m_device(device)
{
    recreateSwapChain();
    createCommandBuffers();
}

Renderer::~Renderer()
{

    freeCommandBuffers();
}

VkCommandBuffer Renderer::beginFrame()
{
    assert(!m_isFrameStarted && "Cannot call beginFrame while already in progress");

    auto result = m_swapChain->acquireNextImage(&m_currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapChain();
        return nullptr;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    m_isFrameStarted = true;
    auto commandBuffer = getCurrentCommandBuffer();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    return commandBuffer;
}

void Renderer::endFrame()
{
    assert(m_isFrameStarted && "Cannot call endFrame while frame is not in progress");
    auto commandBuffer = getCurrentCommandBuffer();


    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }

    auto result = m_swapChain->submitCommandBuffers(&commandBuffer, &m_currentImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || m_window.isWidthResized())
    {
        recreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }


    m_isFrameStarted = false;
    m_currentFrameIndex = (m_currentFrameIndex + 1) % Renderer::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(m_isFrameStarted && "Cannot call beginSwapChainRendererPass if frame is not in progress");
    assert(
        commandBuffer == getCurrentCommandBuffer() &&
        "Cannot begin render pass on command buffer from a different frame"
    );

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapChain->getRenderPass();
    renderPassInfo.framebuffer = m_swapChain->getFrameBuffer(m_currentImageIndex);

    renderPassInfo.renderArea.offset = { 0,0 };
    renderPassInfo.renderArea.extent = m_swapChain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { 0.01f, 0.01f , 0.01f , 0.1f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(m_swapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D sicssor{ {0,0}, m_swapChain->getSwapChainExtent() };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &sicssor);

}

void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(m_isFrameStarted && "Cannot call endSwapChainRendererPass if frame is not in progress");
    assert(
        commandBuffer == getCurrentCommandBuffer() &&
        "Cannot end render pass on command buffer from a different frame"
    );


    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::createCommandBuffers()
{
    m_commandBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);

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

void Renderer::freeCommandBuffers()
{
    vkFreeCommandBuffers(
        m_device.device(),
        m_device.getCommandPool(),
        static_cast<uint32_t>(m_commandBuffers.size()),
        m_commandBuffers.data()
    );
    m_commandBuffers.clear();
}

void Renderer::recreateSwapChain()
{
    auto extent = m_window.getExtent();
    while (extent.width == 0 || extent.height == 0) {
        QCoreApplication::processEvents();
        extent = m_window.getExtent();
    }
    vkDeviceWaitIdle(m_device.device());
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
        std::shared_ptr<SwapChain> spOldSwapchain = std::move(m_swapChain);
        m_swapChain = std::make_unique<SwapChain>(
            m_device,
            extent,
            spOldSwapchain
        );

        if (!spOldSwapchain->compareSwapFormats(*m_swapChain.get()))
        {
            throw std::runtime_error("Swap chain image(or depth) formats has changed");
        }
    }

    //TODO: recreate pipeline
}
