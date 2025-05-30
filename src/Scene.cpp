#include "Scene.h"
#include "const.h"

#include "qglobal.h"
Scene::Scene(Device& device) :
    m_device(device)
{

    VkDeviceSize cmdSize = sizeof(VkDrawIndexedIndirectCommand);
    uint32_t  initSize = 8;


    auto indirectUsageFlags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto indirectMemFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_pointIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        indirectUsageFlags,
        indirectMemFlags,
        0
    );

    m_lineIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        indirectUsageFlags,
        indirectMemFlags,
        0
    );

    m_polygonIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        indirectUsageFlags,
        indirectMemFlags,
        0
    );


    m_stagingBuffer = std::make_unique<Buffer>(
        m_device,
        cmdSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
}

void Scene::addObject(Object&& object)
{

    switch (object.m_model->type())
    {
    case ModelType::Point:
    {
        m_pointObjects.push_back(std::move(object));
        if (m_pointObjects.size() == Layer::MAX_MODEL_COUNT)
        {
            Layer layer(m_device, ModelType::Point, std::move(m_pointObjects));
            m_pointLayers.emplace_back(std::move(layer));

        }
    }
    break;
    case ModelType::Line:
    {
        m_lineObjects.push_back(std::move(object));
        if (m_lineObjects.size() > Layer::MAX_MODEL_COUNT)
        {
            Layer layer(m_device, ModelType::Line, std::move(m_lineObjects));
            m_linelayers.emplace_back(std::move(layer));

        }
    }
    break;
    case ModelType::Polygon:
    {
        m_polygonObjects.push_back(std::move(object));
        if (m_polygonObjects.size() > Layer::MAX_MODEL_COUNT)
        {
            Layer layer(m_device, ModelType::Point, std::move(m_polygonObjects));
            m_polygonLayers.emplace_back(std::move(layer));
        }
    }
    break;
    default:
    break;
    }
}

void Scene::drawPoints(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout)
{
    //for (size_t i = 0; i < m_layers.size(); i++)
    //{
    //    auto& obj = m_pointObjects[i];
    //    auto& model = m_pointObjects[i].m_model;

    //    SimplePushConstantData push{};
    //    push.color = obj.m_color;

    //    push.modelMatrix = obj.m_transform.mat4f();
    //    vkCmdPushConstants(
    //        frameInfo.commandBuffer,
    //        pipelineLayout,
    //        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    //        0,
    //        sizeof(SimplePushConstantData),
    //        &push);


    //    auto& vertexBuffer = model->getVertexBuffer();
    //    auto& indexBuffer = model->getIndexBuffer();

    //    model->bind(frameInfo.commandBuffer);
    //    //model->draw(frameInfo.commandBuffer, frameInfo.camera);
    //    vkCmdDrawIndexedIndirect(
    //        frameInfo.commandBuffer,
    //        m_pointIndirectBuffer->getBuffer(),
    //        i * sizeof(VkDrawIndexedIndirectCommand),
    //        1,
    //        sizeof(VkDrawIndexedIndirectCommand)
    //    );
    ////}
}

void Scene::drawLines(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout)
{

    VkCommandBufferInheritanceInfo inhInfo{};
    inhInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inhInfo.renderPass = frameInfo.renderer.getSwapChainRenderPass();
    inhInfo.subpass = 0;
    inhInfo.framebuffer = frameInfo.renderer.getSwapChainFrameBuffer();

    for (auto& layer : m_linelayers)
    {
        //layer.bind(frameInfo.commandBuffer);
        layer.draw(frameInfo, pipelineLayout, inhInfo);
    }
    //for (size_t i = 0; i < m_lineObjects.size(); i++)
    //{
    //    auto& obj = m_lineObjects[i];
    //    auto& model = m_lineObjects[i].m_model;

    //    SimplePushConstantData push{};
    //    push.color = obj.m_color;

    //    push.modelMatrix = obj.m_transform.mat4f();
    //    vkCmdPushConstants(
    //        frameInfo.commandBuffer,
    //        pipelineLayout,
    //        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    //        0,
    //        sizeof(SimplePushConstantData),
    //        &push);



    //    model->bind(frameInfo.commandBuffer);
    //    //model->draw(frameInfo.commandBuffer, frameInfo.camera);
    //    vkCmdDrawIndexedIndirect(
    //        frameInfo.commandBuffer,
    //        m_lineIndirectBuffer->getBuffer(),
    //        i * sizeof(VkDrawIndexedIndirectCommand),
    //        1,
    //        sizeof(VkDrawIndexedIndirectCommand)
    //    );
    //}

}

void Scene::drawPolygons(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout)
{
    //for (size_t i = 0; i < m_polygonObjects.size(); i++)
    //{
    //    auto& obj = m_polygonObjects[i];
    //    auto& model = m_polygonObjects[i].m_model;

    //    SimplePushConstantData push{};
    //    push.color = obj.m_color;

    //    push.modelMatrix = obj.m_transform.mat4f();
    //    vkCmdPushConstants(
    //        frameInfo.commandBuffer,
    //        pipelineLayout,
    //        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    //        0,
    //        sizeof(SimplePushConstantData),
    //        &push);


    //    auto& vertexBuffer = model->getVertexBuffer();
    //    auto& indexBuffer = model->getIndexBuffer();

    //    model->bind(frameInfo.commandBuffer);
    //    //model->draw(frameInfo.commandBuffer, frameInfo.camera);
    //    vkCmdDrawIndexedIndirect(
    //        frameInfo.commandBuffer,
    //        m_polygonIndirectBuffer->getBuffer(),
    //        i * sizeof(VkDrawIndexedIndirectCommand),
    //        1,
    //        sizeof(VkDrawIndexedIndirectCommand)
    //    );
    //}
}

void Scene::finish()
{
    Layer layer(m_device, ModelType::Line, std::move(m_lineObjects));
    m_linelayers.emplace_back(std::move(layer));

}

void Scene::updateIndrectBuffer(VkDrawIndexedIndirectCommand& cmd,
    std::vector<VkDrawIndexedIndirectCommand>& cmds,
    std::shared_ptr<Buffer>& indircetBuf)
{

    uint32_t count = static_cast<uint32_t>(cmds.size());
    VkDeviceSize instanceSize = sizeof(VkDrawIndexedIndirectCommand);
    VkDeviceSize totalSize = instanceSize * count;
    VkDeviceSize offset = instanceSize * (count - 1);

    uint32_t cap = indircetBuf->getInstanceCount();
    if (count > cap)
    {
        //À©ÈÝ
        uint32_t newCap = qMax(cap * 2, count);
        auto oldBuf = indircetBuf;
        indircetBuf = std::make_shared<Buffer>(
            m_device,
            instanceSize,
            newCap,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0);

        m_device.copyBuffer(oldBuf->getBuffer(), indircetBuf->getBuffer(), cap * instanceSize);

    }



    m_stagingBuffer->map();
    m_stagingBuffer->writeToBuffer((void*)&cmd, instanceSize, 0);
    m_stagingBuffer->flush(instanceSize, /*offset=*/0);
    //stagingBuffer.unmap();

    VkBufferCopy copyRegion{ 0, offset, instanceSize };
    m_device.copyBufferWithInfo(m_stagingBuffer->getBuffer(), indircetBuf->getBuffer(), copyRegion);


}



