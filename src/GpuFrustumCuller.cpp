#include "GpuFrustumCuller.h"

#include <fstream>

GpuFrustumCuller::GpuFrustumCuller(Device& device)
    :m_device(device)
{
    qDebug() << "[GPU Culling] 开始初始化GPU剔除器...";

    // 检查设备能力
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_device.physicalDevice(), &features);

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_device.physicalDevice(), &properties);

    qDebug() << "[GPU Culling] 设备信息:";
    qDebug() << "  设备名称:" << properties.deviceName;
    qDebug() << "  设备类型:" << properties.deviceType;
    qDebug() << "  最大计算工作组调用数:" << properties.limits.maxComputeWorkGroupInvocations;
    qDebug() << "  最大计算工作组数X:" << properties.limits.maxComputeWorkGroupCount[0];
    qDebug() << "  最大计算工作组大小X:" << properties.limits.maxComputeWorkGroupSize[0];

    if (properties.limits.maxComputeWorkGroupInvocations == 0) {
        throw std::runtime_error("设备不支持计算着色器!");
    }
}

GpuFrustumCuller::~GpuFrustumCuller()
{
    if (m_computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.device(), m_computePipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
    }
    if (m_computeShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device.device(), m_computeShader, nullptr);
    }

}

void GpuFrustumCuller::initialize(uint32_t maxObjects)
{
    m_maxObjects = maxObjects;

    createBuffers(maxObjects);
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSets();
    createPipelineLayout();
    createComputePipeline();

    m_isInit = true;

    qDebug() << "[GPU Culling] 初始化完成，最大对象数量:" << maxObjects;
}

void GpuFrustumCuller::performCullingGPUOnly(FrameInfo& frameInfo, const BufferPool& bufferPool)
{

    resetDrawCountBuffers(frameInfo.commandBuffer, bufferPool);

    updateCameraData(frameInfo.camera);

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(frameInfo.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier,
        0, nullptr, 0, nullptr);

    vkCmdBindPipeline(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
    vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout,
        0, 1, &m_descriptorSet, 0, nullptr);


    struct Push {
        uint32_t totalObjects;
        uint32_t segmentCount;
    };

    Push pushConstances;
    pushConstances.totalObjects = m_maxObjects;
    pushConstances.segmentCount = m_segmentAddresses.size();
    vkCmdPushConstants(frameInfo.commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &pushConstances);

    uint32_t workGroups = (m_maxObjects + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
    vkCmdDispatch(frameInfo.commandBuffer, workGroups, 1, 1);

    VkMemoryBarrier finalBarrier{};
    finalBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    finalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(frameInfo.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 1, &finalBarrier, 0, nullptr, 0, nullptr);

    qDebug() << "[GPU-Driven] GPU剔除命令记录完成";
}

void GpuFrustumCuller::resize(uint32_t newMaxObjects)
{
    if (newMaxObjects <= m_maxObjects) {
        return;
    }

    qDebug() << "[GPU Culling]  调整容量:" << m_maxObjects << "->" << newMaxObjects;

    // ⚠ 等待设备空闲
    vkDeviceWaitIdle(m_device.device());

    //  重新创建缓冲区
    createBuffers(newMaxObjects);

    //  重新创建描述符池和描述符集
    createDescriptorPool();      // 重新创建描述符池
    createDescriptorSets();      // 重新分配和绑定描述符集


    m_maxObjects = newMaxObjects;
    qDebug() << "[GPU Culling]  容量调整完成";
}

void GpuFrustumCuller::updateSegmentAddresses(const BufferPool& bufferPool)
{

    m_segmentAddresses.clear();
    // 收集所有Segment的地址信息
    for (const auto& [modelType, segments] : bufferPool.getBufferSegments()) {
        for (size_t i = 0; i < segments.size(); ++i) {
            const auto& segment = segments[i];

            SegmentAddressInfo info{};
            info.drawCommandAddress = segment.getDrawCommandAddress();
            info.drawCountAddress = segment.getDrawCountAddress();
            info.maxDrawCommands = segment.chunks.size();
            info.segmentIndex = static_cast<uint32_t>(i);
            info.modelType = static_cast<uint32_t>(modelType);

            m_segmentAddresses.push_back(info);
        }
    }
    isSegmentNeedsUpdate = false;
    // 上传到GPU
    uploadSegmentAddress();

    qDebug() << "[GPU Culling] Updated" << m_segmentAddresses.size() << "segment addresses";
}

void GpuFrustumCuller::resetDrawCountBuffers(VkCommandBuffer cmd, const BufferPool& bufferPool)
{
    qDebug() << "[GPU Culling] 重置DrawCount缓冲区...";

    // 遍历所有segment，重置drawCount为0
    for (const auto& [modelType, segments] : bufferPool.getBufferSegments()) {
        for (const auto& segment : segments) {
            if (segment.drawCountBuffer) {
                // 使用vkCmdFillBuffer将drawCount重置为0
                vkCmdFillBuffer(cmd, segment.drawCountBuffer->getBuffer(), 0, sizeof(uint32_t), 0);

                qDebug() << "  重置ModelType:" << static_cast<int>(modelType)
                    << "DrawCountBuffer:" << segment.drawCountBuffer->getBuffer();
            }
        }
    }

    // 添加内存屏障，确保重置完成后才能被compute shader读写
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    qDebug() << "[GPU Culling] DrawCount缓冲区重置完成";
}

void GpuFrustumCuller::createBuffers(uint32_t maxObjects)
{
    // 对象数据缓冲区(CPU->GPU)
    m_objectBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(ObjectGPUData),
        maxObjects,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    // 相机数据缓冲区(CPU->GPU)
    m_cameraBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(CameraGPUData),                             // instanceSize
        1,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    m_segmentAddressBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(SegmentAddressInfo),
        MAX_SEGMENTS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
#ifdef DEBUG_BUFFER_ENABLED

    m_debugBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(DebugData),
        1,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_TO_CPU
    );
#endif

}

void GpuFrustumCuller::createDescriptorSetLayout()
{
    qDebug() << "[GPU Culling]  创建描述符布局...";

    m_descriptorSetLayout = DescriptorSetLayout::Builder(m_device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // 对象数据
        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // 相机数据
        .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // Segment地址缓冲区
#ifdef DEBUG_BUFFER_ENABLED
        .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)  // Segment地址缓冲区
#endif
        .build();

    qDebug() << "[GPU Culling]  描述符布局创建完成";
}

void GpuFrustumCuller::createDescriptorPool()
{
    qDebug() << "[GPU Culling]  创建描述符池...";

    m_descriptorPool = DescriptorPool::Builder(m_device)
        .setMaxSets(1)  // 只需要1个描述符集
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4)  // 4个storage buffer
        .build();

    qDebug() << "[GPU Culling]  描述符池创建完成";
}

void GpuFrustumCuller::createComputePipeline()
{
    qDebug() << "[GPU Culling]  创建计算管线...";

    assert(m_pipelineLayout != VK_NULL_HANDLE && "Cannot create pipeline before pipeline layout");

    std::vector<char> computeCode;
    std::ifstream file("E:/project/QtVulkanEngine/bin/Debug/frustum_culling.comp.spv",
        std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("无法打开 frustum_culling.comp.spv 文件!");
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    computeCode.resize(fileSize);
    file.seekg(0);
    file.read(computeCode.data(), fileSize);
    file.close();

    qDebug() << "[GPU Culling] 着色器文件大小:" << fileSize << "字节";

    // 🔧 创建着色器模块
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = computeCode.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(computeCode.data());

    if (vkCreateShaderModule(m_device.device(), &createInfo, nullptr, &m_computeShader) != VK_SUCCESS) {
        throw std::runtime_error("创建计算着色器模块失败!");
    }

    // 🔧 创建计算管线
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = m_computeShader;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = m_pipelineLayout;

    if (vkCreateComputePipelines(m_device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("创建计算管线失败!");
    }

    qDebug() << "[GPU Culling]  计算管线创建完成";
}

void GpuFrustumCuller::uploadObjectDataFromPool(const ObjectManager::ObjectDataPool& dataPool, const BufferPool& bufferPool)
{
    std::vector<ObjectGPUData> objectData(dataPool.m_count);

    for (size_t i = 0; i < dataPool.m_count; i++)
    {
        // 复制变换矩阵
        memcpy(objectData[i].transform,
            &dataPool.m_transforms[i * ObjectManager::FLOAT_NUM_PER_TRANSFORM],
            16 * sizeof(float));

        // 复制边界框
        memcpy(objectData[i].boundingBox,
            &dataPool.m_boundingBoxes[i * ObjectManager::FLOAT_NUM_PER_BOUNDINGBOX],
            4 * sizeof(float));

        objectData[i].chunkId = dataPool.m_chunkIds[i];

        // 从BufferPool获取chunk信息
        const BufferPool::Chunk* chunk = bufferPool.getChunk(dataPool.m_chunkIds[i]);
        if (chunk && chunk->isLoaded) {
            objectData[i].modelType = static_cast<uint32_t>(bufferPool.getChunkType(dataPool.m_chunkIds[i]));
            objectData[i].indexOffset = chunk->indexOffset;
            objectData[i].indexCount = chunk->indexCount;
            objectData[i].vertexOffset = chunk->vertexOffset;
            objectData[i].segmentIndex = chunk->segmentIndex;
        }
        else {
            // 如果chunk不存在或未加载，设置默认值
            qWarning() << "[GPU-Driven] Chunk" << dataPool.m_chunkIds[i] << "不存在或未加载";
            objectData[i].modelType = static_cast<uint32_t>(ModelType::Line);
            objectData[i].indexOffset = 0;
            objectData[i].indexCount = 0;
            objectData[i].vertexOffset = 0;
        }

        objectData[i].visible = 0;
        objectData[i].padding[0] = 0;
    }

    // 上传到GPU
    m_objectBuffer->map();
    m_objectBuffer->writeToBuffer(objectData.data(), sizeof(ObjectGPUData) * dataPool.m_count);
    m_objectBuffer->flush();
    m_objectBuffer->unmap();

    qDebug() << "[GPU-Driven] 对象数据上传完成，数量:" << dataPool.m_count;

    // 调试输出前几个对象的信息
    for (size_t i = 0; i < qMin(dataPool.m_count, size_t(5)); i++) {
        qDebug() << "  对象" << i << ": ChunkId=" << objectData[i].chunkId
            << "Type=" << objectData[i].modelType
            << "IndexOffset=" << objectData[i].indexOffset
            << "IndexCount=" << objectData[i].indexCount
            << "VertexOffset=" << objectData[i].vertexOffset;
    }
}

void GpuFrustumCuller::updateDescriptorSets()
{
    qDebug() << "[GPU Culling]  更新描述符集内容...";

    // 🔧 检查描述符集是否有效
    if (m_descriptorSet == VK_NULL_HANDLE) {
        qDebug() << "[GPU Culling]  描述符集无效，重新创建...";
        createDescriptorSets();
        return;
    }

    // 🔧 获取新缓冲区信息
    auto objectBufferInfo = m_objectBuffer->descriptorInfo();
    auto cameraBufferInfo = m_cameraBuffer->descriptorInfo();
    auto segmentAddressBufferInfo = m_segmentAddressBuffer->descriptorInfo();
    auto debugBufferInfo = m_debugBuffer->descriptorInfo();

    // 🔧 直接使用VkWriteDescriptorSet更新（更安全）
    std::vector<VkWriteDescriptorSet> descriptorWrites(4);

    descriptorWrites[0] = {};
    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = m_descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &objectBufferInfo;

    descriptorWrites[1] = {};
    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = m_descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &cameraBufferInfo;

    descriptorWrites[2] = {};
    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = m_descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &segmentAddressBufferInfo;


    descriptorWrites[3] = {};
    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = m_descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &debugBufferInfo;

    vkUpdateDescriptorSets(m_device.device(), static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(), 0, nullptr);

    qDebug() << "[GPU Culling]  描述符集内容更新完成";
}

void GpuFrustumCuller::createDescriptorSets()
{
    qDebug() << "[GPU Culling]  创建并绑定描述符集...";

    // 🔍 检查前置条件
    if (!m_descriptorPool) {
        throw std::runtime_error("描述符池未创建!");
    }

    if (!m_descriptorSetLayout) {
        throw std::runtime_error("描述符布局未创建!");
    }

    //  获取缓冲区信息
    auto objectBufferInfo = m_objectBuffer->descriptorInfo();
    auto cameraBufferInfo = m_cameraBuffer->descriptorInfo();
    auto segmentAddressBufferInfo = m_segmentAddressBuffer->descriptorInfo();
    auto debugBufferInfo = m_debugBuffer->descriptorInfo();

    //  使用DescriptorWriter创建描述符集
    qDebug() << "[GPU Culling] 开始分配并写入描述符集...";

    bool buildResult = DescriptorWriter(*m_descriptorSetLayout, *m_descriptorPool)
        .writeBuffer(0, &objectBufferInfo)
        .writeBuffer(1, &cameraBufferInfo)
        .writeBuffer(2, &segmentAddressBufferInfo)
        .writeBuffer(3, &debugBufferInfo)
        .build(m_descriptorSet);  //  检查返回值

    qDebug() << "[GPU Culling] 描述符集构建结果:" << (buildResult ? "成功" : "失败");

    if (!buildResult) {
        throw std::runtime_error("构建GPU剔除描述符集失败!");
    }

    if (m_descriptorSet == VK_NULL_HANDLE) {
        throw std::runtime_error("描述符集构建返回成功但句柄为空!");
    }

    qDebug() << "[GPU Culling]  描述符集句柄:" << QString("0x%1").arg(reinterpret_cast<uintptr_t>(m_descriptorSet), 0, 16);
    qDebug() << "[GPU Culling]  描述符集创建并绑定完成";
}

void GpuFrustumCuller::createPipelineLayout()
{
    qDebug() << "[GPU Culling]  创建管线布局...";

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t) * 2;  // 对象数量

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
        m_descriptorSetLayout->getDescriptorSetLayout()
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo,
        nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("创建管线布局失败!");
    }

    qDebug() << "[GPU Culling]  管线布局创建完成";
}

void GpuFrustumCuller::updateCameraData(const Camera& camera)
{
    CameraGPUData cameraData{};

    // 转换Qt矩阵为float数组
    QMatrix4x4 viewMatrix = camera.getView();
    QMatrix4x4 projMatrix = camera.getProjection();

    matrixToFloatArray(viewMatrix, cameraData.viewMatrix);
    matrixToFloatArray(projMatrix, cameraData.projMatrix);

    // 视图矩阵的逆矩阵的第4列就是相机的世界坐标位置
    QMatrix4x4 invViewMatrix = viewMatrix.inverted();
    cameraData.cameraPos[0] = invViewMatrix(0, 3);  // X
    cameraData.cameraPos[1] = invViewMatrix(1, 3);  // Y  
    cameraData.cameraPos[2] = invViewMatrix(2, 3);  // Z

    // 对于透视投影矩阵：
    // near = -m[3][2] / (m[2][2] - 1)
    // far = -m[3][2] / (m[2][2] + 1)
    const float* projData = projMatrix.constData();
    float m22 = projData[10];  // m[2][2]
    float m32 = projData[14];  // m[3][2]

    // 检查是否是透视投影（m[3][2] != 0）
    if (std::abs(m32) > 0.001f) {
        // 透视投影
        cameraData.nearPlane = -m32 / (m22 - 1.0f);
        cameraData.farPlane = -m32 / (m22 + 1.0f);
    }
    else {
        // 正交投影
        cameraData.nearPlane = -(m32 + 1.0f) / m22;
        cameraData.farPlane = -(m32 - 1.0f) / m22;
    }

    cameraData.padding[0] = 0;
    cameraData.padding[1] = 0;
    cameraData.padding[2] = 0;

    if (cameraData.nearPlane > cameraData.farPlane) {
        std::swap(cameraData.nearPlane, cameraData.farPlane);
    }

    // 提取视锥面
    QMatrix4x4 viewProjMatrix = projMatrix * viewMatrix;
    extractFrustumPlanes(viewProjMatrix, cameraData.frustumPlanes);

    if (m_cameraBuffer->map() == VK_SUCCESS) {
        m_cameraBuffer->writeToBuffer(&cameraData, sizeof(CameraGPUData));
        m_cameraBuffer->flush();
        m_cameraBuffer->unmap();
    }
    else {
        throw std::runtime_error("Failed to map CameraBuffer!");
    }

    qDebug() << "[GPU Culling] update CameraData:";
    qDebug() << "  Position:" << cameraData.cameraPos[0] << cameraData.cameraPos[1] << cameraData.cameraPos[2];
    qDebug() << "  NearPlane:" << cameraData.nearPlane << "FarPlane:" << cameraData.farPlane;
}

void GpuFrustumCuller::uploadObjectData(const std::vector<std::shared_ptr<Object>>& objects)
{
    std::vector<ObjectGPUData> objectData(objects.size());

    for (size_t i = 0; i < objects.size(); i++) {
        const auto& obj = objects[i];

        // 转换变换矩阵
        QMatrix4x4 transform = obj->getTransform().mat4f();
        matrixToFloatArray(transform, objectData[i].transform);

        // 获取边界框
        AABB bbox = obj->getBoundingBox();
        objectData[i].boundingBox[0] = bbox.minX;
        objectData[i].boundingBox[1] = bbox.minY;
        objectData[i].boundingBox[2] = bbox.maxX;
        objectData[i].boundingBox[3] = bbox.maxY;

        objectData[i].chunkId = obj->getChunkID();
        objectData[i].modelType = static_cast<uint32_t>(obj->getModel()->type());
        objectData[i].visible = 0;
        //objectData[i].padding = { 0 };
    }

    m_objectBuffer->map();
    m_objectBuffer->writeToBuffer(objectData.data(), sizeof(ObjectGPUData) * objects.size());
    m_objectBuffer->flush();  // ⭐ 确保数据写入GPU可见内存
    m_objectBuffer->unmap();

}

void GpuFrustumCuller::uploadSegmentAddress()
{
    if (!m_segmentAddresses.empty()) {


        VkDeviceSize dataSize = sizeof(SegmentAddressInfo) * m_segmentAddresses.size();
        m_segmentAddressBuffer->map();
        m_segmentAddressBuffer->writeToBuffer(m_segmentAddresses.data(), dataSize);
        m_segmentAddressBuffer->flush();
        m_segmentAddressBuffer->unmap();
    }
}


void GpuFrustumCuller::matrixToFloatArray(const QMatrix4x4& matrix, float* array)
{
    const float* data = matrix.constData();

    // Qt的QMatrix4x4已经是列主序，直接复制即可
    memcpy(array, data, 16 * sizeof(float));
}

void GpuFrustumCuller::extractFrustumPlanes(const QMatrix4x4& viewProjMatrix, float* planes)
{
    const float* m = viewProjMatrix.constData();


    planes[0] = m[3] + m[0];   // (3,0) + (0,0)
    planes[1] = m[7] + m[4];   // (3,1) + (0,1)
    planes[2] = m[11] + m[8];  // (3,2) + (0,2)
    planes[3] = m[15] + m[12]; // (3,3) + (0,3)

    // Right plane: row3 - row0
    planes[4] = m[3] - m[0];   // (3,0) - (0,0)
    planes[5] = m[7] - m[4];   // (3,1) - (0,1)
    planes[6] = m[11] - m[8];  // (3,2) - (0,2)
    planes[7] = m[15] - m[12]; // (3,3) - (0,3)

    // Bottom plane: row3 + row1
    planes[8] = m[3] + m[1];   // (3,0) + (1,0)
    planes[9] = m[7] + m[5];   // (3,1) + (1,1)
    planes[10] = m[11] + m[9]; // (3,2) + (1,2)
    planes[11] = m[15] + m[13];// (3,3) + (1,3)

    // Top plane: row3 - row1
    planes[12] = m[3] - m[1];  // (3,0) - (1,0)
    planes[13] = m[7] - m[5];  // (3,1) - (1,1)
    planes[14] = m[11] - m[9]; // (3,2) - (1,2)
    planes[15] = m[15] - m[13];// (3,3) - (1,3)

    // 对于2D渲染，Near/Far可以简化或忽略
    // Near plane: row2 (如果需要)
    planes[16] = m[2];         // (2,0)
    planes[17] = m[6];         // (2,1)
    planes[18] = m[10];        // (2,2)
    planes[19] = m[14];        // (2,3)

    // Far plane: row3 - row2 (如果需要)
    planes[20] = m[3] - m[2];  // (3,0) - (2,0)
    planes[21] = m[7] - m[6];  // (3,1) - (2,1)
    planes[22] = m[11] - m[10];// (3,2) - (2,2)
    planes[23] = m[15] - m[14];// (3,3) - (2,3)

    // 标准化所有平面
    for (int i = 0; i < 6; i++) {
        int offset = i * 4;
        float length = sqrt(planes[offset] * planes[offset] +
            planes[offset + 1] * planes[offset + 1] +
            planes[offset + 2] * planes[offset + 2]);

        if (length > 0.0001f) {
            planes[offset] /= length;
            planes[offset + 1] /= length;
            planes[offset + 2] /= length;
            planes[offset + 3] /= length;
        }
    }
    // Left plane: 第4列 + 第1列
    //planes[0] = m[12] + m[0];   // m[3][0] + m[0][0]
    //planes[1] = m[13] + m[1];   // m[3][1] + m[0][1]
    //planes[2] = m[14] + m[2];   // m[3][2] + m[0][2]
    //planes[3] = m[15] + m[3];   // m[3][3] + m[0][3]

    //// Right plane: 第4列 - 第1列
    //planes[4] = m[12] - m[0];   // m[3][0] - m[0][0]
    //planes[5] = m[13] - m[1];   // m[3][1] - m[0][1]
    //planes[6] = m[14] - m[2];   // m[3][2] - m[0][2]
    //planes[7] = m[15] - m[3];   // m[3][3] - m[0][3]

    //// Bottom plane: 第4列 + 第2列
    //planes[8] = m[12] + m[4];   // m[3][0] + m[1][0]
    //planes[9] = m[13] + m[5];   // m[3][1] + m[1][1]
    //planes[10] = m[14] + m[6];  // m[3][2] + m[1][2]
    //planes[11] = m[15] + m[7];  // m[3][3] + m[1][3]

    //// Top plane: 第4列 - 第2列
    //planes[12] = m[12] - m[4];  // m[3][0] - m[1][0]
    //planes[13] = m[13] - m[5];  // m[3][1] - m[1][1]
    //planes[14] = m[14] - m[6];  // m[3][2] - m[1][2]
    //planes[15] = m[15] - m[7];  // m[3][3] - m[1][3]

    //// Near plane: 第3列
    //planes[16] = m[8];          // m[2][0]
    //planes[17] = m[9];          // m[2][1]
    //planes[18] = m[10];         // m[2][2]
    //planes[19] = m[11];         // m[2][3]

    //// Far plane: 第4列 - 第3列
    //planes[20] = m[12] - m[8];  // m[3][0] - m[2][0]
    //planes[21] = m[13] - m[9];  // m[3][1] - m[2][1]
    //planes[22] = m[14] - m[10]; // m[3][2] - m[2][2]
    //planes[23] = m[15] - m[11]; // m[3][3] - m[2][3]

    //// 标准化平面方程
    //for (int i = 0; i < 6; i++) {
    //    int offset = i * 4;
    //    QVector3D normal(planes[offset], planes[offset + 1], planes[offset + 2]);
    //    float length = normal.length();

    //    if (length > 0.0f) {
    //        planes[offset] /= length;
    //        planes[offset + 1] /= length;
    //        planes[offset + 2] /= length;
    //        planes[offset + 3] /= length;
    //    }
    //}


    qDebug() << "[GPU Culling] Frustun:";
    const char* planeNames[] = { "Left", "Right", "Bottom", "Top", "Near", "Far" };
    for (int i = 0; i < 6; i++) {
        int offset = i * 4;
        qDebug() << "  " << planeNames[i] << ": ("
            << planes[offset] << ", " << planes[offset + 1] << ", "
            << planes[offset + 2] << ", " << planes[offset + 3] << ")";
    }
}

void GpuFrustumCuller::readDebugData()
{
    DebugData debugData{};

    if (m_debugBuffer->map() == VK_SUCCESS) {
        memcpy(&debugData, m_debugBuffer->getMappedMemory(), sizeof(DebugData));
        m_debugBuffer->unmap();

        qDebug() << "[GPU Debug] ==== GPU剔除调试信息 ====";
        qDebug() << "  处理对象总数:" << debugData.totalProcessed;
        qDebug() << "  可见对象数量:" << debugData.visibleCount;

        for (int i = 0; i < 8; i++) {
            if (debugData.segmentDrawCounts[i] > 0) {
                qDebug() << "  Segment" << i << "绘制数量:" << debugData.segmentDrawCounts[i];
            }
        }
        qDebug() << "DrawCountBuffer 中的数量:" << debugData.drawIndex;
        qDebug() << "[GPU Debug] ================================";
    }
}
