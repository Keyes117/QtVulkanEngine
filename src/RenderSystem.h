#pragma once

#include "Camera.h"
#include "const.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"
#include "Pipeline.h"
#include "SwapChain.h"
#include <memory>
#include <vector>

class RenderSystem
{
public:
    RenderSystem(Device& device, VkRenderPass renderPass);
    ~RenderSystem();


    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;

    RenderSystem(RenderSystem&&) = delete;
    RenderSystem& operator=(RenderSystem&&) = delete;

    void renderObjects(VkCommandBuffer commandBuffer, std::vector<Object>& objects, const Camera& camera);

private:
    void createPipelineLayout();
    void createPipeline(VkRenderPass renderPass);


private:
    Device& m_device;

    std::unique_ptr<Pipeline>       m_pipeline;
    VkPipelineLayout                m_pipelineLayout;


};

