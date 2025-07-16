#include "RequestManager.h"
#include <QDebug>
#include <cassert>
#include <chrono>

RequestManager::RequestManager(Device& device) : m_device(device) {
    qDebug() << "[RequestManager] create RequestManager";
}

RequestManager::~RequestManager() {
    cleanup();
}

void RequestManager::initialize() {
    qDebug() << "[RequestManager] Initializing GPU-CPU communication system...";

    try {
        createBuffers();
        createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
        updateDescriptorSets();
        createSyncObjects();

        // Persistently map buffers
        m_mappedRequests = static_cast<GPURequest*>(m_requestBuffer->getMappedMemory());
        m_mappedResponses = static_cast<CPUResponse*>(m_responseBuffer->getMappedMemory());
        m_mappedCounters = static_cast<RequestCounters*>(m_counterBuffer->getMappedMemory());

        // Initialize counters
        m_mappedCounters->requestCount = 0;
        m_mappedCounters->responseCount = 0;
        m_mappedCounters->frameNumber = 0;

        m_initialized = true;
        qDebug() << "[RequestManager] Initialization complete!";
    }
    catch (const std::exception& e) {
        qCritical() << "[RequestManager] Initialization failed:" << e.what();
        cleanup();
        throw;
    }
}

void RequestManager::cleanup() {
    if (!m_initialized) return;

    qDebug() << "[RequestManager] Cleaning up resources...";

    // Wait for GPU to finish
    if (m_device.device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device.device());
    }

    // Clean up pipeline layout
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }

    // Unmap buffers
    if (m_requestBuffer && m_mappedRequests) {
        m_requestBuffer->unmap();
        m_mappedRequests = nullptr;
    }
    if (m_responseBuffer && m_mappedResponses) {
        m_responseBuffer->unmap();
        m_mappedResponses = nullptr;
    }
    if (m_counterBuffer && m_mappedCounters) {
        m_counterBuffer->unmap();
        m_mappedCounters = nullptr;
    }

    // Destroy synchronization objects
    destroySyncObjects();

    // Destroy buffers
    m_requestBuffer.reset();
    m_responseBuffer.reset();
    m_counterBuffer.reset();

    // Destroy descriptors
    m_descriptorSetLayout.reset();
    m_descriptorPool.reset();

    m_initialized = false;
    qDebug() << "[RequestManager] Cleanup complete";
}

// Main interface for frame processing
void RequestManager::processFrame(FrameInfo& frameInfo) {
    if (!m_initialized) return;

    std::lock_guard<std::mutex> lock(m_processingMutex);
    m_currentFrame = frameInfo.frameIndex;

    // Update push constants
    vkCmdPushConstants(
        frameInfo.commandBuffer,
        m_pipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(uint32_t),
        &m_totalObjects
    );

    // Reset counters
    resetCounters(frameInfo.commandBuffer);

    // Update frame number
    m_mappedCounters->frameNumber = frameInfo.frameIndex;

    // Memory barrier to ensure GPU can see the data
    insertMemoryBarrier(frameInfo.commandBuffer);

    // Read and process requests from the previous frame
    readAndProcessRequests();
}

void RequestManager::resetCounters(VkCommandBuffer cmd) {
    // Reset GPU request counter
    vkCmdFillBuffer(cmd, m_counterBuffer->getBuffer(),
        offsetof(RequestCounters, requestCount),
        sizeof(uint32_t), 0);

    // Reset response counter
    vkCmdFillBuffer(cmd, m_counterBuffer->getBuffer(),
        offsetof(RequestCounters, responseCount),
        sizeof(uint32_t), 0);

    // Memory barrier
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void RequestManager::readAndProcessRequests() {
    uint32_t requestCount = m_mappedCounters->requestCount;
    if (requestCount == 0) return;

    qDebug() << "[RequestManager] Frame" << m_currentFrame << "received" << requestCount << "requests";

    m_stats.lastFrameRequests = requestCount;
    m_stats.totalRequests += requestCount;

    // Process each request
    for (uint32_t i = 0; i < requestCount && i < MAX_GPU_REQUESTS; ++i) {
        const GPURequest& request = m_mappedRequests[i];

        // Validate request
        if (request.isValid()) {
            processRequest(request, i);
            m_stats.processedRequests++;
        }
        else {
            qWarning() << "[RequestManager] Invalid request" << i << "checksum mismatch";
            addResponse(i, ResponseStatus::FAILED);
            m_stats.failedRequests++;
        }
    }

    qDebug() << "[RequestManager] Processing complete, generated" << m_mappedCounters->responseCount << "responses";
}

void RequestManager::processRequest(const GPURequest& request, uint32_t requestIndex) {
    RequestType type = static_cast<RequestType>(request.type);
    RequestPriority priority = static_cast<RequestPriority>(request.priority);

    qDebug() << "[RequestManager] Processing request" << requestIndex
        << " Type:" << (int)type
        << " ObjectID:" << request.objectID
        << " Priority:" << (int)priority;

    // Find and execute handler
    auto it = m_handlers.find(type);
    if (it != m_handlers.end()) {
        try {
            it->second(request);
            addResponse(requestIndex, ResponseStatus::SUCCESS);
        }
        catch (const std::exception& e) {
            qWarning() << "[RequestManager] Handler exception:" << e.what();
            addResponse(requestIndex, ResponseStatus::FAILED);
        }
    }
    else {
        qWarning() << "[RequestManager] No handler found for type" << (int)type;
        addResponse(requestIndex, ResponseStatus::FAILED);
    }
}

void RequestManager::addResponse(uint32_t requestIndex, ResponseStatus status,
    uint32_t dataOffset, uint32_t dataSize) {
    uint32_t responseIndex = m_mappedCounters->responseCount;
    if (responseIndex >= MAX_CPU_RESPONSES) {
        qWarning() << "[RequestManager] Response buffer full";
        return;
    }

    CPUResponse& response = m_mappedResponses[responseIndex];
    response.requestID = requestIndex;
    response.status = static_cast<uint32_t>(status);
    response.dataOffset = dataOffset;
    response.dataSize = dataSize;
    response.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    response.frameCompleted = m_currentFrame;

    // Increment response counter
    m_mappedCounters->responseCount++;
}

void RequestManager::insertMemoryBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// Buffer creation
void RequestManager::createBuffers() {
    qDebug() << "[RequestManager] Creating communication buffers...";

    // GPU request buffer (GPU write, CPU read)
    m_requestBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(GPURequest),
        MAX_GPU_REQUESTS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU
    );

    // CPU response buffer (CPU write, GPU read)
    m_responseBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(CPUResponse),
        MAX_CPU_RESPONSES,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    // Counter buffer (dual access)
    m_counterBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(RequestCounters),
        1,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    qDebug() << "[RequestManager] Buffers created successfully";
    qDebug() << "  RequestBuffer:" << REQUEST_BUFFER_SIZE << "bytes";
    qDebug() << "  ResponseBuffer:" << RESPONSE_BUFFER_SIZE << "bytes";
    qDebug() << "  CounterBuffer:" << COUNTER_BUFFER_SIZE << "bytes";
}

// System descriptor
void RequestManager::createDescriptorSetLayout() {
    qDebug() << "[RequestManager] Creating descriptor set layout...";

    // Create push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t);  // totalObjects

    // Create descriptor layout
    m_descriptorSetLayout = DescriptorSetLayout::Builder(m_device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // RequestBuffer
        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // ResponseBuffer
        .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // CounterBuffer
        .build();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
        m_descriptorSetLayout->getDescriptorSetLayout()
    };
    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    qDebug() << "[RequestManager] Pipeline layout created successfully";
}

void RequestManager::createDescriptorPool() {
    qDebug() << "[RequestManager] Creating descriptor pool...";

    m_descriptorPool = DescriptorPool::Builder(m_device)
        .setMaxSets(1)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3)
        .build();

    qDebug() << "[RequestManager] Descriptor pool created successfully";
}

void RequestManager::createDescriptorSets() {
    qDebug() << "[RequestManager] Creating descriptor sets...";

    bool success = m_descriptorPool->allocateDescriptorSet(
        m_descriptorSetLayout->getDescriptorSetLayout(),
        m_descriptorSet
    );

    if (!success) {
        throw std::runtime_error("RequestManager: Descriptor set allocation failed!");
    }

    qDebug() << "[RequestManager] Descriptor sets created successfully";
}

void RequestManager::updateDescriptorSets() {
    qDebug() << "[RequestManager] Updating descriptor sets...";

    VkDescriptorBufferInfo requestBufferInfo{};
    requestBufferInfo.buffer = m_requestBuffer->getBuffer();
    requestBufferInfo.offset = 0;
    requestBufferInfo.range = REQUEST_BUFFER_SIZE;

    VkDescriptorBufferInfo responseBufferInfo{};
    responseBufferInfo.buffer = m_responseBuffer->getBuffer();
    responseBufferInfo.offset = 0;
    responseBufferInfo.range = RESPONSE_BUFFER_SIZE;

    VkDescriptorBufferInfo counterBufferInfo{};
    counterBufferInfo.buffer = m_counterBuffer->getBuffer();
    counterBufferInfo.offset = 0;
    counterBufferInfo.range = COUNTER_BUFFER_SIZE;

    DescriptorWriter writer(*m_descriptorSetLayout, *m_descriptorPool);
    writer.writeBuffer(0, &requestBufferInfo);
    writer.writeBuffer(1, &responseBufferInfo);
    writer.writeBuffer(2, &counterBufferInfo);
    writer.overwrite(m_descriptorSet);

    qDebug() << "[RequestManager] Descriptor sets updated successfully";
}

// System interface
VkDescriptorSetLayout RequestManager::getDescriptorSetLayout() const {
    return m_descriptorSetLayout ? m_descriptorSetLayout->getDescriptorSetLayout() : VK_NULL_HANDLE;
}

void RequestManager::bindDescriptorSet(VkCommandBuffer cmd, uint32_t setIndex) {
    if (m_descriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_pipelineLayout,  // Use our own pipeline layout
            setIndex,
            1,
            &m_descriptorSet,
            0,
            nullptr
        );
    }
}

// Handler registration
void RequestManager::registerHandler(RequestType type, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(m_processingMutex);
    m_handlers[type] = handler;
    qDebug() << "[RequestManager] Registered handler for type:" << (int)type;
}

// Synchronization objects
void RequestManager::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_requestSemaphore);
    vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_responseSemaphore);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateFence(m_device.device(), &fenceInfo, nullptr, &m_syncFence);
}

void RequestManager::destroySyncObjects() {
    if (m_requestSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device.device(), m_requestSemaphore, nullptr);
        m_requestSemaphore = VK_NULL_HANDLE;
    }
    if (m_responseSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device.device(), m_responseSemaphore, nullptr);
        m_responseSemaphore = VK_NULL_HANDLE;
    }
    if (m_syncFence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device.device(), m_syncFence, nullptr);
        m_syncFence = VK_NULL_HANDLE;
    }
}

// Logging interface
void RequestManager::logStatistics() const {
    qDebug() << "[RequestManager] ===== Statistics =====";
    qDebug() << "   Total requests:" << m_stats.totalRequests;
    qDebug() << "   Processed requests:" << m_stats.processedRequests;
    qDebug() << "   Failed requests:" << m_stats.failedRequests;
    qDebug() << "   Last frame requests:" << m_stats.lastFrameRequests;
    qDebug() << "   Current frame:" << m_currentFrame;
    if (m_mappedCounters) {
        qDebug() << "   Pending requests:" << m_mappedCounters->requestCount;
    }
}

uint32_t RequestManager::getPendingRequestCount() const {
    return m_mappedCounters ? m_mappedCounters->requestCount : 0;
}

// Request interface
bool RequestManager::hasNewRequests() {
    return m_mappedCounters && m_mappedCounters->requestCount > 0;
}

std::vector<GPURequest> RequestManager::getAndClearRequests() {
    std::vector<GPURequest> requests;
    if (!m_mappedCounters) return requests;

    uint32_t count = m_mappedCounters->requestCount;
    for (uint32_t i = 0; i < count && i < MAX_GPU_REQUESTS; ++i) {
        requests.push_back(m_mappedRequests[i]);
    }

    return requests;
}

void RequestManager::submitResponse(const CPUResponse& response) {
    if (!m_mappedResponses || !m_mappedCounters) return;

    uint32_t index = m_mappedCounters->responseCount;
    if (index < MAX_CPU_RESPONSES) {
        m_mappedResponses[index] = response;
        m_mappedCounters->responseCount++;
    }
}