#pragma once

#include "Camera.h"
#include "Renderer.h"
#include <vulkan/vulkan.h>

struct FrameInfo
{
    int frameIndex;
    float frameTime;
    VkCommandBuffer commandBuffer;
    Camera& camera;
    VkDescriptorSet globalDescriptorSet;
};

