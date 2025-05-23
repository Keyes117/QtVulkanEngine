#pragma once

#include "Buffer.h"
#include "Device.h"
#include "Camera.h"

#include <qvector2d.h>
#include <vector>

enum class ModelType
{
    Point = 0,
    Line,
    Polygon
};

class Model
{
public:

    struct Vertex
    {
        QVector3D position{};
        QVector3D color{};

        static std::vector<VkVertexInputBindingDescription> getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescription();
    };

    struct Builder
    {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
        ModelType           type;
    };

    struct Chunk
    {
        uint32_t        firstIndex;
        uint32_t        indexCount;
        QVector2D       minXY, maxXY;
    };

    Model(Device& device, const Model::Builder& builder);
    ~Model();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer, const Camera& camera);

    uint32_t  vertexCount()const { return m_vertexCount; }
    uint32_t  indexCount() const { return m_indexCount; }
    ModelType type() const { return m_type; }
    const std::shared_ptr<Buffer>& getVertexBuffer() const { return m_vertexBuffer; }
    const std::shared_ptr<Buffer>& getIndexBuffer() const { return m_indexBuffer; }

    // Not copyable or movable
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;
private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
    //void buildChunks(const Model::Builder& builder);
private:
    Device& m_device;

    ModelType                   m_type;
    //std::vector<Model::Chunk>   m_chunks;

    std::shared_ptr<Buffer>     m_vertexBuffer;
    uint32_t                    m_vertexCount;

    bool                        m_hasIndexBuffer;
    std::shared_ptr<Buffer>     m_indexBuffer;
    uint32_t                    m_indexCount;
};


