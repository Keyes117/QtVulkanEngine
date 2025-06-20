#include "RenderSystem.h"

#include <array>
#include <QApplication>
#include <qtimer.h>



RenderSystem::RenderSystem(Device& device,
    VkRenderPass renderPass,
    VkDescriptorSetLayout globalSetLayout,
    VkPrimitiveTopology topology /*= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST*/)
    :m_device(device),
    m_topology(topology)
{
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass, topology);
}

RenderSystem::~RenderSystem()
{
    vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
}

void RenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayout{ globalSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayout.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayout.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo,
        nullptr, &m_pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}

void RenderSystem::createPipeline(VkRenderPass renderPass, VkPrimitiveTopology topology)
{
    assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

    PipelineConfigInfo pipelineConfigInfo{};
    Pipeline::setPipelineConfigInfo(pipelineConfigInfo, topology);

    pipelineConfigInfo.renderPass = renderPass;
    pipelineConfigInfo.pipelineLayout = m_pipelineLayout;
    m_pipeline = std::make_unique<Pipeline>(
        m_device,
        "simple_shader.vert.spv",
        "simple_shader.frag.spv",
        pipelineConfigInfo
    );
}

