#pragma once
#include "Buffer.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"

class Scene
{

public:
    explicit Scene(Device& device);
    ~Scene() = default;

    void draw(VkCommandBuffer commandBuffer);

    void addObject(Object&& object);
    void removeObject(size_t index);



    //void buildChunks(const Model::Builder& builder);
private:
    void buildIndirectCommands();
    void buildSingleIndirectCommands(const std::vector<Object>& objects, std::shared_ptr<Buffer>& buffer);
private:
    Device& m_device;
    //std::vector<Model::Chunk>   m_chunks;

    std::vector<Object>                                     m_pointObjects;              // ªÊª≠∂‘œÛ
    std::vector<Object>                                     m_lineObjects;
    std::vector<Object>                                     m_polygonObjects;

    std::shared_ptr<Buffer>                                 m_pointIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_lineIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_polygonIndirectBuffer;
};

