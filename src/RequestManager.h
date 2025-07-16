#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <functional>

#include "Device.h"
#include "VMABuffer.h"
#include "Descriptors.h"
#include "FrameInfo.h"
#include "const.h"

class RequestManager
{
public:
    RequestManager(Device& device);
    ~RequestManager();

    // Core interface
    void initialize();
    void cleanup();
    bool isInitialized() const { return m_initialized; }

    // Main processing interface - called in render loop
    void processFrame(FrameInfo& frameInfo);

    // Descriptor interface - for shader use
    VkDescriptorSet getDescriptorSet() const { return m_descriptorSet; }
    VkDescriptorSetLayout getDescriptorSetLayout() const;
    void bindDescriptorSet(VkCommandBuffer cmd, uint32_t setIndex = 1);
    
    // Object count management
    void setTotalObjects(uint32_t count) { m_totalObjects = count; }
    uint32_t getTotalObjects() const { return m_totalObjects; }

    // Request handler interface
    using RequestHandler = std::function<void(const GPURequest& request)>;
    void registerHandler(RequestType type, RequestHandler handler);

    // Debug and statistics interface
    void logStatistics() const;
    uint32_t getPendingRequestCount() const;

    // Legacy interface (optional)
    bool hasNewRequests();
    std::vector<GPURequest> getAndClearRequests();
    void submitResponse(const CPUResponse& response);

private:
    // Buffer management
    void createBuffers();
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void updateDescriptorSets();

    // Render loop internal operations
    void resetCounters(VkCommandBuffer cmd);
    void readAndProcessRequests();
    void processRequest(const GPURequest& request, uint32_t requestIndex);
    void addResponse(uint32_t requestIndex, ResponseStatus status,
        uint32_t dataOffset = 0, uint32_t dataSize = 0);

    // Synchronization management
    void createSyncObjects();
    void destroySyncObjects();
    void insertMemoryBarrier(VkCommandBuffer cmd);

private:
    Device& m_device;
    bool m_initialized = false;
    uint32_t m_currentFrame = 0;

    // Descriptor system
    std::unique_ptr<DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<DescriptorPool> m_descriptorPool;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;  // Pipeline layout for push constants

    // Communication buffers
    std::unique_ptr<VMABuffer> m_requestBuffer;     // GPU to CPU
    std::unique_ptr<VMABuffer> m_responseBuffer;    // CPU to GPU
    std::unique_ptr<VMABuffer> m_counterBuffer;     // Dual access

    // Persistently mapped pointers
    GPURequest* m_mappedRequests = nullptr;
    CPUResponse* m_mappedResponses = nullptr;
    RequestCounters* m_mappedCounters = nullptr;

    // Request handling
    std::unordered_map<RequestType, RequestHandler> m_handlers;
    mutable std::mutex m_processingMutex;
    uint32_t m_totalObjects = 0;  // Total number of objects for shader processing

    // Synchronization objects
    VkSemaphore m_requestSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_responseSemaphore = VK_NULL_HANDLE;
    VkFence m_syncFence = VK_NULL_HANDLE;

    // Statistics
    struct Statistics {
        uint32_t totalRequests = 0;
        uint32_t processedRequests = 0;
        uint32_t failedRequests = 0;
        uint32_t lastFrameRequests = 0;
    } m_stats;
};