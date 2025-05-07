#include "RenderSystem.h"

#include <array>
#include <QApplication>
#include <qtimer.h>

RenderSystem::RenderSystem(Device& device, VkRenderPass renderPass)
    :m_device(device)
{
    createPipelineLayout();
    createPipeline(renderPass);
}

RenderSystem::~RenderSystem()
{
    vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
}

void RenderSystem::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);


    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo,
        nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void RenderSystem::createPipeline(VkRenderPass renderPass)
{
    assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfigInfo{};
    Pipeline::defaultPipelineConfigInfo(pipelineConfigInfo);

    pipelineConfigInfo.renderPass = renderPass;
    pipelineConfigInfo.pipelineLayout = m_pipelineLayout;
    m_pipeline = std::make_unique<Pipeline>(
        m_device,
        "simple_shader.vert.spv",
        "simple_shader.frag.spv",
        pipelineConfigInfo
    );
}

void RenderSystem::renderObjects(VkCommandBuffer commandBuffer, std::vector<Object>& objects)
{
    int i = 0;
    for (auto& obj : objects)
    {
        i += 1;

        float angle = std::fmod(obj.m_transform2d.rotation + 0.01f * i, 2.f * M_PI);
        if (angle < 0) {
            angle += 2 * M_PI; // Ensure non-negative
        }
        obj.m_transform2d.rotation = angle;
    }

    m_pipeline->bind(commandBuffer);

    for (auto& obj : objects)
    {

        SimplePushConstantData push{};
        push.offset = obj.m_transform2d.translation;
        push.color = obj.m_color;
        push.transform = obj.m_transform2d.mat2f();
        vkCmdPushConstants(
            commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);
        obj.m_model->bind(commandBuffer);
        obj.m_model->draw(commandBuffer);
    }
}

