#include "Scene.h"
#include "const.h"

Scene::Scene(Device& device) :
    m_device(device)
{

    VkDeviceSize cmdSize = sizeof(VkDrawIndexedIndirectCommand);
    uint32_t  initSize = 8;
    auto usageFlags = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto memFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;



    m_pointIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        usageFlags,
        memFlags,
        0
    );

    m_lineIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        usageFlags,
        memFlags,
        0
    );

    m_polygonIndirectBuffer = std::make_shared<Buffer>(
        m_device,
        cmdSize,
        initSize,
        usageFlags,
        memFlags,
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

    auto updateIndirect = [&](
        std::vector<Object>& objects,
        std::vector<VkDrawIndexedIndirectCommand>& cmds,
        std::shared_ptr<Buffer>& buf)
        {
            objects.push_back(std::move(object));
            cmds.push_back(cmd);
            uint32_t count = static_cast<uint32_t>(cmds.size());
            VkDeviceSize instanceSize = sizeof(VkDrawIndexedIndirectCommand);
            VkDeviceSize totalSize = instanceSize * count;

            uint32_t cap = buf->getInstanceCount();
            if (count > cap)
            {
                //À©ÈÝ
                uint32_t newCap = qMax(cap * 2, count);
                auto newBuf = std::make_shared<Buffer>(
                    m_device,
                    instanceSize,
                    newCap,
                    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    0);

                newBuf->map(totalSize);
                newBuf->writeToBuffer(cmds.data(), totalSize);
                newBuf->flush(totalSize, 0);
                newBuf->unmap();
                buf = std::move(newBuf);
            }
            else
            {
                VkDeviceSize singleSize = sizeof(cmd);
                VkDeviceSize offset = singleSize * (count - 1);

                buf->map(singleSize, offset);
                buf->writeToBuffer(&cmds.back(), singleSize, offset);
                buf->flush(singleSize, offset);
                buf->unmap();
            }

        };

    switch (object.m_model->type())
    {
    case ModelType::Point:
    updateIndirect(m_pointObjects, m_pointCmd, m_pointIndirectBuffer);
    break;
    case ModelType::Line:
    updateIndirect(m_lineObjects, m_lineCmd, m_pointIndirectBuffer);
    break;
    case ModelType::Polygon:
    updateIndirect(m_polygonObjects, m_polygonCmd, m_polygonIndirectBuffer);
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



void Scene::updateIndirect(Object& objects, std::vector<VkDrawIndirectCommand>& cmds, std::shared_ptr<Buffer>& buf)
{

}

