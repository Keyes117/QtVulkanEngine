#pragma once
#include "Buffer.h"
#include "Model.h"
#include "Device.h"
#include "Object.h"
#include "VMABuffer.h"


struct Tile
{
    AABB bounds;
    std::vector<Object*> objects;
    VkCommandBuffer secondaryCommandBuffer = VK_NULL_HANDLE;
};

class Layer
{
public:
    static constexpr size_t MAX_MODEL_COUNT = 10000;

public:
    Layer(Device& device, ModelType type, std::vector<Object>&& object);
    ~Layer();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer, const Camera& camera);

    void drawTiles(VkCommandBuffer primary, const Camera& camera, VkCommandBufferInheritanceInfo& inhInfo);
    void recordVisibleTileCommands(const Camera& camera, VkCommandBufferInheritanceInfo& inhInfo);

    std::vector<Tile> getTiles() const { return m_tiles; }

    std::shared_ptr<VMABuffer> getVertexBuffer() { return m_vertexBuffer; }
    std::shared_ptr<VMABuffer> getIndexBuffer() { return m_indexBuffer; }

private:
    void init();

    void createTiles(int cols, int rows);
    void assignObjectsToTiles();


    void createVertexBuffers(const std::vector<Model::Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);

private:
    Device& m_device;
    ModelType                   m_type;
    AABB                        m_worldBounds;

    std::vector<Object>         m_objects;
    std::vector<Tile>           m_tiles;

    std::shared_ptr<VMABuffer>  m_vertexBuffer;
    uint32_t                    m_vertexCount{ 0 };

    bool                        m_hasIndexBuffer;
    std::shared_ptr<VMABuffer>  m_indexBuffer;
    uint32_t                    m_indexCount{ 0 };
};

