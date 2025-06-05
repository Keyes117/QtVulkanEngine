#pragma once

#include "Camera.h"
#include "const.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"
#include "Pipeline.h"
#include "SwapChain.h"
#include "FrameInfo.h"
#include "Scene.h"
#include <memory>
#include <vector>

class RenderSystem
{
public:
    RenderSystem(Device& device,
        VkRenderPass renderPass,
        VkDescriptorSetLayout globalSetLayout,
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    ~RenderSystem();


    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    //RenderSystem(RenderSystem&&) = delete;
    //RenderSystem& operator=(RenderSystem&&) = delete;

    void bind(VkCommandBuffer commandBuffer)
    {
        m_pipeline->bind(commandBuffer);
    }

    //void render()
    VkPipelineLayout getPipelineLayout() { return m_pipelineLayout; }

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass, VkPrimitiveTopology topology);


private:
    Device& m_device;

    VkPrimitiveTopology             m_topology;
    std::unique_ptr<Pipeline>       m_pipeline;
    VkPipelineLayout                m_pipelineLayout;


};

