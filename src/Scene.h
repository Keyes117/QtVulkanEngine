#pragma once
#include "Buffer.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"
#include "Layer.h"
#include "FrameInfo.h"
class Scene
{

public:
    explicit Scene(Device& device);
    ~Scene() = default;

    void addObject(Object&& object);

    void drawPoints(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);

    void drawLines(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);

    void drawPolygons(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout);

    void finish();
private:
    void updateIndrectBuffer(VkDrawIndexedIndirectCommand& cmd,
        std::vector<VkDrawIndexedIndirectCommand>& cmds,
        std::shared_ptr<Buffer>& indircetBuf);

private:
    Device& m_device;

    std::vector<Layer>                                      m_pointLayers;
    std::vector<Layer>                                      m_linelayers;
    std::vector<Layer>                                      m_polygonLayers;

    std::vector<Object>                                     m_pointObjects;              // ªÊª≠∂‘œÛ
    std::vector<Object>                                     m_lineObjects;
    std::vector<Object>                                     m_polygonObjects;

    std::shared_ptr<Buffer>                                 m_pointIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_lineIndirectBuffer;
    std::shared_ptr<Buffer>                                 m_polygonIndirectBuffer;

    std::vector<VkDrawIndexedIndirectCommand>               m_pointCmd;
    std::vector<VkDrawIndexedIndirectCommand>               m_lineCmd;
    std::vector<VkDrawIndexedIndirectCommand>               m_polygonCmd;

    std::unique_ptr<Buffer>                                 m_stagingBuffer;

};

