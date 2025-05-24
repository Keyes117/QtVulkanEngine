#pragma once

#include "const.h"
#include "Device.h"
#include "Model.h"
#include "MyVulkanWindow.h"
#include "SwapChain.h"

#include <cassert>
#include <memory>
#include <vector>

class Renderer
{

    struct Chunk
    {
        uint32_t        firstIndex;
        uint32_t        indexCount;
        QVector2D       minXY, maxXY;
    };

public:
    Renderer(MyVulkanWindow& window, Device& device);
    ~Renderer();

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;


    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    VkRenderPass getSwapChainRenderPass() const { return m_swapChain->getRenderPass(); }
    bool isFrameInProgress() const { return m_isFrameStarted; }

    float  getAspectRatio() const { return m_swapChain->extentAspectRatio(); }

    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(isFrameInProgress() && "Cannot get command buffer when frame not in progress");
        return m_commandBuffers[m_currentFrameIndex];
    }

    int getFrameIndex() const
    {
        assert(m_isFrameStarted && "Cannot get frame index when frame not in progress");
        return m_currentFrameIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();


private:
    MyVulkanWindow& m_window;
    Device& m_device;
    std::unique_ptr<SwapChain>      m_swapChain;
    std::vector<VkCommandBuffer>    m_commandBuffers;

    uint32_t                        m_currentImageIndex;
    int                             m_currentFrameIndex;
    bool                            m_isFrameStarted{ false };

};

