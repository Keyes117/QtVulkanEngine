#pragma once

#include "Camera.h"
#include "const.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"
#include "Pipeline.h"
#include "SwapChain.h"
#include "FrameInfo.h"
#include <memory>
#include <vector>

class RenderSystem
{
public:
    RenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~RenderSystem();


    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    //RenderSystem(RenderSystem&&) = delete;
    //RenderSystem& operator=(RenderSystem&&) = delete;

    void renderObjects(FrameInfo& frameInfo, std::vector<Object>& objects);

private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);


private:
    Device& m_device;

    std::unique_ptr<Pipeline>       m_pipeline;
    VkPipelineLayout                m_pipelineLayout;


};

