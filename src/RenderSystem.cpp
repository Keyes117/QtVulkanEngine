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

void RenderSystem::renderObjects(FrameInfo& frameInfo, std::vector<Object>& objects)
{
    //int i = 0;
    //for (auto& obj : objects)
    //{
    //    i += 1;
    //    float angle = std::fmod(obj.m_transform.rotation.z() + 0.1f * 1, 360.0f);
    //    if (angle < 0) {
    //        angle += 2 * M_PI; // Ensure non-negative
    //    }
    //    obj.m_transform.rotation.setZ(angle);
    //}

    m_pipeline->bind(frameInfo.commandBuffer);

    auto projectView = frameInfo.camera.getProjection() * frameInfo.camera.getView();

    for (auto& obj : objects)
    {

        SimplePushConstantData push{};
        push.color = obj.m_color;

        //obj.m_transform.rotation.setY(std::fmod(obj.m_transform.rotation.y() + 0.01f, 2 * M_PI));
        //obj.m_transform.rotation.setX(std::fmod(obj.m_transform.rotation.x() + 0.005f, 2 * M_PI));

        push.transform = projectView * obj.m_transform.mat4f();
        vkCmdPushConstants(
            frameInfo.commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);
        obj.m_model->bind(frameInfo.commandBuffer);
        obj.m_model->draw(frameInfo.commandBuffer);
    }
}

