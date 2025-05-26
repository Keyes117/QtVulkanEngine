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

}

void Scene::addObject(Object&& object)
{

    VkDrawIndexedIndirectCommand cmd{};
    cmd.indexCount = object.m_model->indexCount();
    cmd.instanceCount = 1;
    cmd.firstIndex = 0;
    cmd.vertexOffset = 0;
    cmd.firstInstance = 0;

    switch (object.m_model->type())
    {
    case ModelType::Point:
    {
        m_pointObjects.push_back(std::move(object));
        m_pointCmd.push_back(cmd);
        updateIndrectBuffer(cmd, m_pointCmd, m_pointIndirectBuffer);

    }
    break;
    case ModelType::Line:
    {
        m_lineObjects.push_back(std::move(object));
        m_lineCmd.push_back(cmd);
        updateIndrectBuffer(cmd, m_lineCmd, m_lineIndirectBuffer);
    }
    break;
    case ModelType::Polygon:
    {
        m_polygonObjects.push_back(std::move(object));
        m_polygonCmd.push_back(cmd);
        updateIndrectBuffer(cmd, m_polygonCmd, m_polygonIndirectBuffer);
    }
    break;
    default:
    break;
    }
}

void Scene::drawPoints(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
    for (size_t i = 0; i < m_pointObjects.size(); i++)
    {
        auto& obj = m_pointObjects[i];
        auto& model = m_pointObjects[i].m_model;

        SimplePushConstantData push{};
        push.color = obj.m_color;

        push.modelMatrix = obj.m_transform.mat4f();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);


        auto& vertexBuffer = model->getVertexBuffer();
        auto& indexBuffer = model->getIndexBuffer();

        model->bind(commandBuffer);
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            m_pointIndirectBuffer->getBuffer(),
            i * sizeof(VkDrawIndexedIndirectCommand),
            1,
            sizeof(VkDrawIndexedIndirectCommand)
        );
    }
}

void Scene::drawLines(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
    for (size_t i = 0; i < m_lineObjects.size(); i++)
    {
        auto& obj = m_lineObjects[i];
        auto& model = m_lineObjects[i].m_model;

        SimplePushConstantData push{};
        push.color = obj.m_color;

        push.modelMatrix = obj.m_transform.mat4f();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);



        model->bind(commandBuffer);
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            m_lineIndirectBuffer->getBuffer(),
            i * sizeof(VkDrawIndexedIndirectCommand),
            1,
            sizeof(VkDrawIndexedIndirectCommand)
        );
    }

}

void Scene::drawPolygons(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
{
    for (size_t i = 0; i < m_polygonObjects.size(); i++)
    {
        auto& obj = m_polygonObjects[i];
        auto& model = m_polygonObjects[i].m_model;

        SimplePushConstantData push{};
        push.color = obj.m_color;

        push.modelMatrix = obj.m_transform.mat4f();
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);


        auto& vertexBuffer = model->getVertexBuffer();
        auto& indexBuffer = model->getIndexBuffer();

        model->bind(commandBuffer);
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            m_polygonIndirectBuffer->getBuffer(),
            i * sizeof(VkDrawIndexedIndirectCommand),
            1,
            sizeof(VkDrawIndexedIndirectCommand)
        );
    }
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

    Buffer stagingBuffer(
        m_device,
        instanceSize,
        1,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void*)&cmd, instanceSize, 0);
    stagingBuffer.flush(instanceSize, /*offset=*/0);
    stagingBuffer.unmap();

    VkBufferCopy copyRegion{ 0, offset, instanceSize };
    m_device.copyBufferWithInfo(stagingBuffer.getBuffer(), indircetBuf->getBuffer(), copyRegion);


}



