#include "Scene.h"

Scene::Scene(Device& device)
m_device(device)
{

}

void Scene::addObject(Object&& object)
{
    auto type = object.m_model->type();
    switch (type)
    {
    case ModelType::Point:
    {
        m_pointObjects.push_back(std::move(object));
    }
    break;
    case ModelType::Line:
    {
        m_lineObjects.push_back(std::move(object));
    }
    break;
    case ModelType::Polygon:
    {
        m_polygonObjects.push_back(std::move(object));
    }
    break;
    default: break;
    }


    buildIndirectCommands();
}

void Scene::buildIndirectCommands()
{

}

void Scene::buildSingleIndirectCommands(const std::vector<Object>& objects, std::shared_ptr<Buffer>& buffer)
{

    uint32_t count = static_cast<uint32_t>(objects.size());
    std::vector<VkDrawIndexedIndirectCommand> cmds(count);
    for (uint32_t i = 0; i < count; i++)
    {
        auto& model = objects[i].m_model;

        VkDrawIndexedIndirectCommand cmd;
        cmd.indexCount = model->indexCount();
        cmd.instanceCount = 1;
        cmd.firstIndex = 0;
        cmd.vertexOffset = 0;
        cmd.firstInstance = 0;

        cmds[i] = cmd;
    }

    VkDeviceSize size = sizeof(VkDrawIndexedIndirectCommand) * count;

}
