#pragma once
#include "Buffer.h"
#include "Model.h"
#include "Device.h"
#include "Object.h"
#include "VMABuffer.h"
#include "FrameInfo.h"

struct Tile
{
    AABB bounds;
    std::vector<Object*>    objects;
    VkCommandBuffer secondaryCommandBuffer = VK_NULL_HANDLE;
};

class Layer
{
public:
    static constexpr size_t MAX_MODEL_COUNT = 10000;

public:
    Layer(Device& device, ModelType type, std::vector<Object>&& object);
    ~Layer();

    /* void bind(VkCommandBuffer commandBuffer);*/
    void draw(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout, VkCommandBufferInheritanceInfo& inhInfo);

    //void drawTiles(FrameInfo& frameInfo, VkCommandBufferInheritanceInfo& inhInfo);
    void recordVisibleTileCommands(FrameInfo& camera, VkPipelineLayout pipelineLayout, VkCommandBufferInheritanceInfo& inhInfo);

    std::vector<Tile> getTiles() const { return m_tiles; }

    /* std::shared_ptr<VMABuffer> getVertexBuffer() { return m_vertexBuffer; }
     std::shared_ptr<VMABuffer> getIndexBuffer() { return m_indexBuffer; }*/

     // 移动构造函数
    Layer(Layer&& other) noexcept
        : m_device(other.m_device)
        , m_type(other.m_type)
        , m_objects(std::move(other.m_objects))
        , m_tiles(std::move(other.m_tiles))
        , m_tileCols(other.m_tileCols)
        , m_tileRows(other.m_tileRows)
        , m_worldBounds(other.m_worldBounds)
        , m_vertexCount(other.m_vertexCount)
        , m_indexCount(other.m_indexCount)
    {
        // 确保其他对象的析构函数不会释放资源
        other.m_tiles.clear();
    }

    // 移动赋值运算符
    Layer& operator=(Layer&& other) noexcept {
        if (this != &other) {
            // 先释放当前对象的资源
            for (auto& tile : m_tiles) {
                if (tile.secondaryCommandBuffer != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(m_device.device(), m_device.getCommandPool(), 1, &tile.secondaryCommandBuffer);
                }
            }

            // 移动新资源
            m_type = other.m_type;
            m_objects = std::move(other.m_objects);
            m_tiles = std::move(other.m_tiles);
            m_tileCols = other.m_tileCols;
            m_tileRows = other.m_tileRows;
            m_worldBounds = other.m_worldBounds;
            m_vertexCount = other.m_vertexCount;
            m_indexCount = other.m_indexCount;

            // 确保其他对象的析构函数不会释放资源
            other.m_tiles.clear();
        }
        return *this;
    }

private:
    void init();

    void initBuffer();
    void initTiles();
    void assignObjectsToTiles();


    //void createVertexBuffers(const std::vector<Model::Vertex>& vertices);
    //void createIndexBuffers(const std::vector<uint32_t>& indices);

private:
    Device& m_device;
    ModelType                   m_type;
    AABB                        m_worldBounds;
    int                         m_tileCols = 16;
    int                         m_tileRows = 16;

    std::vector<Object>         m_objects;
    std::vector<Tile>           m_tiles;

    //std::shared_ptr<VMABuffer>  m_vertexBuffer;
    uint32_t                    m_vertexCount{ 0 };

    //bool                        m_hasIndexBuffer;
    //std::shared_ptr<VMABuffer>  m_indexBuffer;
    uint32_t                    m_indexCount{ 0 };
};

