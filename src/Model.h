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

struct AABB
{
    float minX, minY;
    float maxX, maxY;

    AABB() :minX(0), minY(0), maxX(0), maxY(0) {}
    AABB(float minx, float miny, float maxx, float maxy)
        :minX(minx), minY(miny), maxX(maxx), maxY(maxy) {}

    bool overlaps(const AABB& other)
    {
        return !(maxX < other.minX || minX > other.maxX ||
            maxY < other.minY || minY > other.maxY);
    }
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

    Model() = default;
    Model(Model::Builder& builder);
    ~Model();

    const std::vector<Vertex>& getVerticeRef() const { return m_vertices; }
    const std::vector<uint32_t>& getIndicesRef() const { return m_indices; }
    AABB getBoundingBox() { return m_boundingBox; }
    uint32_t  vertexCount()const { return m_vertices.size(); }
    uint32_t  indexCount() const { return m_indices.size(); }
    ModelType type() const { return m_type; }

private:
    ModelType                   m_type;
    std::vector<Vertex>         m_vertices;
    std::vector<uint32_t>       m_indices;
    AABB                        m_boundingBox{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };
};


