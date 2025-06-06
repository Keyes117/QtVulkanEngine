#pragma once

#include "MyVulkanWindow.h"

// std lib headers
#include <string>
#include <vector>


#include "vma/vk_mem_alloc.h"


struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    bool graphicsFamilyHasValue = false;
    bool presentFamilyHasValue = false;
    bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class Device
{

public:
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    Device(MyVulkanWindow& window);
    ~Device();

    // Not copyable or movable
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;

    VkCommandPool getCommandPool() { return m_commandPool; }
    VkDevice device() { return m_VkDevice; }
    VkSurfaceKHR surface() { return m_VkSurface; }
    VkQueue graphicsQueue() { return m_graphicsQueue; }
    VkQueue presentQueue() { return m_presentQueue; }
    VkPhysicalDevice physicalDevice() { return m_physicalDevice; }
    VmaAllocator allocator() const { return m_allocator; }

    // 添加获取图形队列族索引的方法
    uint32_t getGraphicsQueueFamily() { 
        return findPhysicalQueueFamilies().graphicsFamily; 
    }

    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(m_physicalDevice); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(m_physicalDevice); }
    VkFormat findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Buffer Helper Functions
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);

    void createBufferVMA(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        VkBuffer& buffer,
        VmaAllocation& allocation
    );

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void copyBufferWithInfo(VkBuffer srcBuffer, VkBuffer dstBuffer, VkBufferCopy copyInfo);
    void copyBufferToImage(
        VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

    void createImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory);

    VkPhysicalDeviceProperties properties;

private:
    void createInstance();

    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();

    void initVulkanMemAllocator();
    void initVulkanProfiler();

    // helper functions
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    void hasGflwRequiredInstanceExtensions();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    std::vector<const char*> getRequiredExtensions();
    void checkInstanceExtensionSupport(const std::vector<const char*>& required);

    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    MyVulkanWindow& m_window;
    VkCommandPool m_commandPool;

    VmaAllocator    m_allocator;

    VkDevice m_VkDevice;
    VkSurfaceKHR m_VkSurface;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
    const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};