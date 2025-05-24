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

    void addObject(Object&& object);

    void drawPoints(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void drawLines(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void drawPolygons(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    //void buildChunks(const Model::Builder& builder);
private:
    void updateIndirect(Object& objects,
        std::vector<VkDrawIndirectCommand>& cmds,
        std::shared_ptr<Buffer>& buf);
private:
    Device& m_device;
    //std::vector<Model::Chunk>   m_chunks;

    std::vector<Object>                                     m_pointObjects;              // ªÊª≠∂‘œÛ
    std::vector<Object>                                     m_lineObjects;
    std::vector<Object>                                     m_polygonObjects;

    std::shared_ptr<Buffer>                                 m_pointIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_lineIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_polygonIndirectBuffer;

    std::vector<VkDrawIndexedIndirectCommand>               m_pointCmd;
    std::vector<VkDrawIndexedIndirectCommand>               m_lineCmd;
    std::vector<VkDrawIndexedIndirectCommand>               m_polygonCmd;


};

