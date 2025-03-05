﻿/**
 * @file i_vulkan_text_renderer.h
 * @author Christian Saloň
 */

#pragma once

#include <memory>

#include <vulkan/vulkan.h>

#include "glyph_cache.h"
#include "text_block.h"
#include "text_renderer.h"
#include "text_renderer_utils.h"

namespace vft {

/**
 * @class IVulkanTextRenderer
 *
 * @brief Interface for Vulkan text renderers
 */
class IVulkanTextRenderer : public TextRenderer {
public:
    virtual ~IVulkanTextRenderer() = default;

    // Setters for vulkan objects
    virtual void setPhysicalDevice(VkPhysicalDevice physicalDevice) = 0;
    virtual void setLogicalDevice(VkDevice logicalDevice) = 0;
    virtual void setCommandPool(VkCommandPool commandPool) = 0;
    virtual void setGraphicsQueue(VkQueue graphicsQueue) = 0;
    virtual void setRenderPass(VkRenderPass renderPass) = 0;
    virtual void setCommandBuffer(VkCommandBuffer commandBuffer) = 0;

    // Getters for vulkan objects
    virtual VkPhysicalDevice getPhysicalDevice() = 0;
    virtual VkDevice getLogicalDevice() = 0;
    virtual VkCommandPool getCommandPool() = 0;
    virtual VkQueue getGraphicsQueue() = 0;
    virtual VkRenderPass getRenderPass() = 0;
    virtual VkCommandBuffer getCommandBuffer() = 0;
};

}  // namespace vft
