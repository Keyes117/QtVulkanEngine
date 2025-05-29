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
}

Layer::~Layer()
{
    for (auto& tile : m_tiles)
    {
        vkFreeCommandBuffers(m_device.device(), m_device.getCommandPool(), 1, &tile.secondaryCommandBuffer);
    }
}


void Layer::bind(VkCommandBuffer commandBuffer)
{
    VkBuffer buffers[] = { m_vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (m_hasIndexBuffer)
    {
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void Layer::draw(VkCommandBuffer commandBuffer, const Camera& camera)
{
    if (!m_hasIndexBuffer)
    {
        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
    }
    else
    {
        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
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

        m_vertexCount += obj.m_model.vertexCount();
        m_indexCount += obj.m_model.indexCount();
    }

    vertices.reserve(m_vertexCount);
    indices.reserve(m_indexCount);

    for (int i = 0; i < m_objects.size(); i++)
    {


        auto& obj = m_objects[i];
        auto& model = obj.m_model;

        uint32_t offset = static_cast<uint32_t>(vertices.size());
        vertices.insert(vertices.end(), model.getVerticeRef().begin(), model.getVerticeRef().end());

        for (auto& index : model.getIndicesRef())
        {
            if (index == std::numeric_limits<uint32_t>::max())
                indices.push_back(index);
            else
                indices.push_back(offset + index);
        }
    }

    createVertexBuffers(vertices);
    createIndexBuffers(indices);

}

void Layer::createVertexBuffers(const std::vector<Model::Vertex>& vertices)
{
    //assert(m_vertexCount >= 3 && "Vertex Count must be at lease 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;

    uint32_t vertexSize = sizeof(vertices[0]);

    VMABuffer stagingBuffer(
        m_device,
        vertexSize,
        m_vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)vertices.data());

    m_vertexBuffer = std::make_shared<VMABuffer>(
        m_device,
        vertexSize,
        m_vertexCount,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );


    m_device.copyBuffer(stagingBuffer.getBuffer(), m_vertexBuffer->getBuffer(), bufferSize);
}

void Layer::createIndexBuffers(const std::vector<uint32_t>& indices)
{
    m_hasIndexBuffer = m_indexCount > 0;
    //assert(m_indexCount >= 3 && "Vertex Count must be at lease 3");

    if (!m_hasIndexBuffer)
        return;

    VkDeviceSize bufferSize = sizeof(indices[0]) * m_indexCount;
    uint32_t indexSize = sizeof(indices[0]);

    VMABuffer stagingBuffer{
           m_device,
           indexSize,
           m_indexCount,
           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
           VMA_MEMORY_USAGE_CPU_TO_GPU
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)indices.data());


    m_indexBuffer = std::make_shared<VMABuffer>(
        m_device,
        indexSize,
        m_indexCount,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    m_device.copyBuffer(stagingBuffer.getBuffer(), m_indexBuffer->getBuffer(), bufferSize);
}