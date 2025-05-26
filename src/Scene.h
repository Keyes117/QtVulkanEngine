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

private:
    void updateIndrectBuffer(VkDrawIndexedIndirectCommand& cmd,
        std::vector<VkDrawIndexedIndirectCommand>& cmds,
        std::shared_ptr<Buffer>& indircetBuf);

private:
    Device& m_device;

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

