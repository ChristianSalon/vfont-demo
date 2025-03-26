﻿/**
 * @file vulkan_sdf_text_renderer.h
 * @author Christian Saloň
 */

#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "font_atlas.h"
#include "glyph_cache.h"
#include "sdf_tessellator.h"
#include "sdf_text_renderer.h"
#include "vulkan_text_renderer.h"

namespace vft {

/**
 * @brief Basic implementation of vulkan text renderer where charactrers are rendered using signed distance fields
 * stored in font atlases
 */
class VulkanSdfTextRenderer : public VulkanTextRenderer, public SdfTextRenderer {
public:
    /**
     * @brief Represents a font atlas texture containing vulkan objects used to render charcaters
     */
    class FontTexture {
    public:
        VkImage image{nullptr};                 /**< Vulkan image containing font texture */
        VkDeviceMemory memory{nullptr};         /**< Vulkan memory containing the font texture */
        VkImageView imageView{nullptr};         /**< Vulkan image view of font texture */
        VkSampler sampler{nullptr};             /**< Vulkan sampler of font texture */
        VkDescriptorSet descriptorSet{nullptr}; /**< Vulkan descriptor set of font texure */

        FontTexture(VkImage image,
                    VkDeviceMemory memory,
                    VkImageView imageView,
                    VkSampler sampler,
                    VkDescriptorSet descriptorSet)
            : image{image}, memory{memory}, imageView{imageView}, sampler{sampler}, descriptorSet{descriptorSet} {}
    };

protected:
    /**
     * Hash map containing font textures of selected font atlases containng info about glyphs (key: Font family, value:
     * FontTexture object)
     */
    std::unordered_map<std::string, FontTexture> _fontTextures{};

    VkBuffer _vertexBuffer{nullptr};                       /**< Vulkan vertex buffer */
    VkDeviceMemory _vertexBufferMemory{nullptr};           /**< Vulkan vertex buffer memory */
    VkBuffer _boundingBoxIndexBuffer{nullptr};             /**< Vulkan index buffer for bonding boxes */
    VkDeviceMemory _boundingBoxIndexBufferMemory{nullptr}; /**< Vulkan index buffer memory for bonding boxes  */

    VkPipelineLayout _pipelineLayout{nullptr}; /**< Vulkan pipeline layout for rendering glyphs using sdfs */
    VkPipeline _pipeline{nullptr};             /**< Vulkan pipeline for rendering glyphs using sdfs */

    VkDescriptorSetLayout _fontAtlasDescriptorSetLayout{nullptr}; /**< Vulkan descriptor set layout for font atlases */
    std::vector<VkDescriptorSet> _fontAtlasDescriptorSets{};      /**< Vulkan descriptor sets for font atlases */

public:
    VulkanSdfTextRenderer(VkPhysicalDevice physicalDevice,
                          VkDevice logicalDevice,
                          VkQueue graphicsQueue,
                          VkCommandPool commandPool,
                          VkRenderPass renderPass,
                          VkCommandBuffer commandBuffer = nullptr);
    virtual ~VulkanSdfTextRenderer();

    void draw() override;
    void update() override;

    void addFontAtlas(const FontAtlas &atlas) override;

protected:
    void _createVertexAndIndexBuffers();
    void _createPipeline();

    void _createDescriptorPool() override;
    void _createFontAtlasDescriptorSetLayout();
    VkDescriptorSet _createFontAtlasDescriptorSet(VkImageView imageView, VkSampler sampler);

    void _copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void _transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
};

}  // namespace vft
