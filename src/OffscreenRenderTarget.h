#pragma once

#include "Device.h"
#include <memory>

class OffscreenRenderTarget
{
public:
    OffscreenRenderTarget(Device& device);
    ~OffscreenRenderTarget();

    void createResource(VkExtent2D& extend);
    void destroyResource();

    void beginRenderPass(VkCommandBuffer cmd);
    void endRednerPass(VkCommandBuffer cmd);

    void copyToSwapChain(VkCommandBuffer cmd, VkImage swapChainIamge, VkExtent2D extent);

    VkRenderPass getRenderPass() const { return m_renderPass; }
    VkFramebuffer getFramebuffer() const { return m_framebuffer; }
    VkImage getColorImage() const { return m_colorImage; }
    VkImageView getColorImageView() const { return m_colorImageView; }

    bool isInitialized() const { return m_initialized; }

private:
    Device& m_device;
    bool m_initialized{ false };

    VkImage m_colorImage{ VK_NULL_HANDLE };
    VkDeviceMemory m_colorImageMemory{ VK_NULL_HANDLE };
    VkImageView m_colorImageView{ VK_NULL_HANDLE };
    VkFramebuffer m_framebuffer{ VK_NULL_HANDLE };
    VkRenderPass m_renderPass{ VK_NULL_HANDLE };

    VkExtent2D m_extent{ 0, 0 };

    void createColorResources(VkExtent2D extent);
    void createRenderPass();
    void createFramebuffer();
};

