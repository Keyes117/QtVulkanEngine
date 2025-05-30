#include "Layer.h"

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

Layer::Layer(Device& device, ModelType type, std::vector<Object>&& object) :
    m_device(device),
    m_type(type),
    m_objects(std::move(object))
{
    init();
    initTiles();
    assignObjectsToTiles();
}

Layer::~Layer()
{
    for (auto& tile : m_tiles)
    {
        if (tile.secondaryCommandBuffer != VK_NULL_HANDLE)
            vkFreeCommandBuffers(m_device.device(), m_device.getCommandPool(), 1, &tile.secondaryCommandBuffer);
    }
}


//void Layer::bind(VkCommandBuffer commandBuffer)
//{
//    VkBuffer buffers[] = { m_vertexBuffer->getBuffer() };
//    VkDeviceSize offsets[] = { 0 };
//    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
//
//    if (m_hasIndexBuffer)
//    {
//        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
//    }
//}

void Layer::draw(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, VkCommandBufferInheritanceInfo& inhInfo)
{
    recordVisibleTileCommands(frameInfo, pipelineLayout, inhInfo);
    std::vector<VkCommandBuffer> exec;
    for (auto& tile : m_tiles)
    {
        if (!tile.objects.empty() && frameInfo.camera.getFrustum2D().insersects(tile.bounds))
        {
            exec.push_back(tile.secondaryCommandBuffer);
        }
    }

    if (!exec.empty())
    {
        vkCmdExecuteCommands(frameInfo.commandBuffer, static_cast<uint32_t>(exec.size()), exec.data());
    }

}

void Layer::recordVisibleTileCommands(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, VkCommandBufferInheritanceInfo& inhInfo)
{
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
        | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    beginInfo.pInheritanceInfo = &inhInfo;
    for (auto& tile : m_tiles)
    {
        if (!tile.objects.empty() && frameInfo.camera.getFrustum2D().insersects(tile.bounds))
        {
            vkResetCommandBuffer(tile.secondaryCommandBuffer, 0);
            vkBeginCommandBuffer(tile.secondaryCommandBuffer, &beginInfo);


            for (auto* obj : tile.objects)
            {
                SimplePushConstantData push{};
                push.color = obj->m_color;

                push.modelMatrix = obj->m_transform.mat4f();
                vkCmdPushConstants(
                    tile.secondaryCommandBuffer,
                    pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    0,
                    sizeof(SimplePushConstantData),
                    &push);

                obj->m_model->bind(tile.secondaryCommandBuffer);
                obj->m_model->draw(tile.secondaryCommandBuffer, frameInfo.camera);
            }
            vkEndCommandBuffer(tile.secondaryCommandBuffer);
        }
    }
}

void Layer::init()
{
    std::vector<Model::Vertex> vertices;
    std::vector<uint32_t> indices;
    m_vertexCount = 0;
    m_indexCount = 0;

    m_worldBounds = AABB(1e9f, 1e9f, -1e9f, -1e9f);

    // 2. 创建顶点/索引缓冲

    for (auto& obj : m_objects)
    {
        AABB b = obj.getBoundingBox();
        m_worldBounds.minX = std::min(m_worldBounds.minX, b.minX);
        m_worldBounds.minY = std::min(m_worldBounds.minY, b.minY);
        m_worldBounds.maxX = std::max(m_worldBounds.maxX, b.maxX);
        m_worldBounds.maxY = std::max(m_worldBounds.maxY, b.maxY);

        m_vertexCount += obj.m_model->vertexCount();
        m_indexCount += obj.m_model->indexCount();
    }

    vertices.reserve(m_vertexCount);
    indices.reserve(m_indexCount);

    //for (int i = 0; i < m_objects.size(); i++)
    //{
    //    auto& obj = m_objects[i];
    //    auto& model = obj.m_model;

    //    uint32_t offset = static_cast<uint32_t>(vertices.size());
    //    vertices.insert(vertices.end(), model.getVerticeRef().begin(), model.getVerticeRef().end());

    //    for (auto& index : model.getIndicesRef())
    //    {
    //        if (index == std::numeric_limits<uint32_t>::max())
    //            indices.push_back(index);
    //        else
    //            indices.push_back(offset + index);
    //    }
    //}

    //createVertexBuffers(vertices);
    //createIndexBuffers(indices);

}

void Layer::initBuffer()
{

}

void Layer::initTiles()
{
    float totalWidth = m_worldBounds.maxX - m_worldBounds.minX;
    float totalHeight = m_worldBounds.maxY - m_worldBounds.minY;

    float w = totalWidth / m_tileCols;
    float h = totalHeight / m_tileRows;

    m_tiles.clear();
    m_tiles.reserve(m_tileCols * m_tileRows);

    for (int y = 0; y < m_tileRows; y++)
    {
        for (int x = 0; x < m_tileCols; x++)
        {
            Tile tile;
            tile.bounds = AABB(
                m_worldBounds.minX + x * w,
                m_worldBounds.minY + y * h,
                m_worldBounds.minX + (x + 1) * w,
                m_worldBounds.minY + (y + 1) * h);

            VkCommandBufferAllocateInfo allocateInfo;
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
            allocateInfo.commandPool = m_device.getCommandPool();
            allocateInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(m_device.device(), &allocateInfo, &tile.secondaryCommandBuffer);
            m_tiles.push_back(tile);
        }
    }
}

void Layer::assignObjectsToTiles()
{
    for (auto& tile : m_tiles)
    {
        tile.objects.clear();
    }

    for (auto& obj : m_objects)
    {
        AABB b = obj.getBoundingBox();
        for (auto& t : m_tiles)
        {
            if (b.overlaps(t.bounds))
            {
                t.objects.push_back(&obj);
            }
        }
    }
}

//void Layer::createVertexBuffers(const std::vector<Model::Vertex>& vertices)
//{
//    //assert(m_vertexCount >= 3 && "Vertex Count must be at lease 3");
//    VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;
//
//    uint32_t vertexSize = sizeof(vertices[0]);
//
//    VMABuffer stagingBuffer(
//        m_device,
//        vertexSize,
//        m_vertexCount,
//        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//        VMA_MEMORY_USAGE_CPU_TO_GPU
//    );
//
//    stagingBuffer.map();
//    stagingBuffer.writeToBuffer((void*)vertices.data());
//
//    m_vertexBuffer = std::make_shared<VMABuffer>(
//        m_device,
//        vertexSize,
//        m_vertexCount,
//        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//        VMA_MEMORY_USAGE_GPU_ONLY
//    );
//
//
//    m_device.copyBuffer(stagingBuffer.getBuffer(), m_vertexBuffer->getBuffer(), bufferSize);
//}

//void Layer::createIndexBuffers(const std::vector<uint32_t>& indices)
//{
//    m_hasIndexBuffer = m_indexCount > 0;
//    //assert(m_indexCount >= 3 && "Vertex Count must be at lease 3");
//
//    if (!m_hasIndexBuffer)
//        return;
//
//    VkDeviceSize bufferSize = sizeof(indices[0]) * m_indexCount;
//    uint32_t indexSize = sizeof(indices[0]);
//
//    VMABuffer stagingBuffer{
//           m_device,
//           indexSize,
//           m_indexCount,
//           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//           VMA_MEMORY_USAGE_CPU_TO_GPU
//    };
//
//    stagingBuffer.map();
//    stagingBuffer.writeToBuffer((void*)indices.data());
//
//
//    m_indexBuffer = std::make_shared<VMABuffer>(
//        m_device,
//        indexSize,
//        m_indexCount,
//        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//        VMA_MEMORY_USAGE_GPU_ONLY
//    );
//
//    m_device.copyBuffer(stagingBuffer.getBuffer(), m_indexBuffer->getBuffer(), bufferSize);
//}