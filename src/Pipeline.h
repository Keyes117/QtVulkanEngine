#pragma once

#include <string>
#include <vector>

#include "Device.h"
#include "Model.h"

enum class PipelineType
{
    Graphics,
    Compute
};

struct PipelineConfigInfo
{

    PipelineConfigInfo(const PipelineConfigInfo&) = delete;
    PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

    VkPipelineViewportStateCreateInfo   viewportInfo;
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo rasterizationInfo;
    VkPipelineMultisampleStateCreateInfo multisampleInfo;
    VkPipelineColorBlendAttachmentState colorBlendAttachment;
    VkPipelineColorBlendStateCreateInfo colorBlendInfo;
    VkPipelineDepthStencilStateCreateInfo depthStencilInfo;

    std::vector<VkDynamicState> dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo dynamicStateInfo;

    VkPipelineLayout pipelineLayout = nullptr;
    VkRenderPass renderPass = nullptr;
    uint32_t subpass = 0;
};

struct ComputePipelineConfigInfo
{
    ComputePipelineConfigInfo(const ComputePipelineConfigInfo&) = delete;
    ComputePipelineConfigInfo& operator=(const ComputePipelineConfigInfo&) = delete;

    VkPipelineLayout pipelineLayout = nullptr;
};

class Pipeline
{
public:
    Pipeline(Device& device, const std::string& vertFilePath,
        const std::string& fragFilePath, const PipelineConfigInfo& config);

    ~Pipeline();

    Pipeline(const Pipeline&) = default;
    Pipeline& operator=(const Pipeline&) = default;

    void bind(VkCommandBuffer commandBuffer);

    static void setPipelineConfigInfo(PipelineConfigInfo& configInfo, VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

private:
    static std::vector<char> readFile(const std::string& filePath);

    void createGraphicPipeline(const Device& device,
        const std::string& vertFilePath,
        const std::string& fragFilePath,
        const PipelineConfigInfo& config);

    void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

    Device& m_device;
    VkPipeline      m_graphicsPipeline;
    VkShaderModule  m_vertShaderModule;
    VkShaderModule  m_fragShaderModule;
};

