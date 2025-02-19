﻿/**
 * @file combined_drawer.cpp
 * @author Christian Saloň
 */

#include <glm/vec4.hpp>

#include "combined_drawer.h"

namespace vft {

CombinedDrawer::CombinedDrawer(GlyphCache &cache) : Drawer{cache} {};

CombinedDrawer::~CombinedDrawer() {
    // Destroy vulkan buffers
    if (this->_lineSegmentsIndexBuffer != nullptr)
        this->_destroyBuffer(this->_lineSegmentsIndexBuffer, this->_lineSegmentsIndexBufferMemory);

    if (this->_curveSegmentsIndexBuffer != nullptr)
        this->_destroyBuffer(this->_curveSegmentsIndexBuffer, this->_curveSegmentsIndexBufferMemory);

    if (this->_vertexBuffer != nullptr)
        this->_destroyBuffer(this->_vertexBuffer, this->_vertexBufferMemory);

    vkDestroyPipeline(this->_vulkanContext.logicalDevice, this->_lineSegmentsPipeline, nullptr);
    vkDestroyPipelineLayout(this->_vulkanContext.logicalDevice, this->_lineSegmentsPipelineLayout, nullptr);

    vkDestroyPipeline(this->_vulkanContext.logicalDevice, this->_curveSegmentsPipeline, nullptr);
    vkDestroyPipelineLayout(this->_vulkanContext.logicalDevice, this->_curveSegmentsPipelineLayout, nullptr);
}

void CombinedDrawer::init(VulkanContext vulkanContext) {
    Drawer::init(vulkanContext);

    this->_createLineSegmentsPipeline();
    this->_createCurveSegmentsPipeline();
}

void CombinedDrawer::draw(std::vector<std::shared_ptr<TextBlock>> textBlocks, VkCommandBuffer commandBuffer) {
    // Check if there are characters to render
    if (this->_vertices.size() == 0) {
        return;
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->_lineSegmentsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->_lineSegmentsPipelineLayout, 0, 1,
                            &this->_uboDescriptorSet, 0, nullptr);

    VkBuffer vertexBuffers[] = {this->_vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Draw line segments
    vkCmdBindIndexBuffer(commandBuffer, this->_lineSegmentsIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    for (int i = 0; i < textBlocks.size(); i++) {
        for (Character &character : textBlocks[i]->getCharacters()) {
            if (character.glyph.mesh.getVertexCount() > 0) {
                GlyphKey key{textBlocks.at(i)->getFont()->getFontFamily(), character.getUnicodeCodePoint(), 0};

                vft::CharacterPushConstants pushConstants{character.getModelMatrix(), textBlocks.at(i)->getColor()};
                vkCmdPushConstants(commandBuffer, this->_lineSegmentsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(vft::CharacterPushConstants), &pushConstants);

                vkCmdDrawIndexed(commandBuffer, character.glyph.mesh.getIndexCount(GLYPH_MESH_TRIANGLE_BUFFER_INDEX), 1,
                                 this->_offsets.at(key).at(LINE_OFFSET_BUFFER_INDEX), 0, 0);
            }
        }
    }

    // Draw curve segments
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->_curveSegmentsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->_curveSegmentsPipelineLayout, 0, 1,
                            &this->_uboDescriptorSet, 0, nullptr);

    // vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, this->_curveSegmentsIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    for (int i = 0; i < textBlocks.size(); i++) {
        for (Character &character : textBlocks[i]->getCharacters()) {
            if (character.glyph.mesh.getVertexCount() > 0) {
                GlyphKey key{textBlocks.at(i)->getFont()->getFontFamily(), character.getUnicodeCodePoint(), 0};

                vft::CharacterPushConstants pushConstants{character.getModelMatrix(), textBlocks.at(i)->getColor()};
                vkCmdPushConstants(commandBuffer, this->_curveSegmentsPipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                   sizeof(vft::CharacterPushConstants), &pushConstants);

                ViewportPushConstants viewportPushConstants{this->_viewportWidth, this->_viewportHeight};
                vkCmdPushConstants(commandBuffer, this->_curveSegmentsPipelineLayout,
                                   VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, sizeof(vft::CharacterPushConstants),
                                   sizeof(ViewportPushConstants), &viewportPushConstants);

                vkCmdDrawIndexed(commandBuffer, character.glyph.mesh.getIndexCount(GLYPH_MESH_CURVE_BUFFER_INDEX), 1,
                                 this->_offsets.at(key).at(CURVE_OFFSET_BUFFER_INDEX), 0, 0);
            }
        }
    }
}

void CombinedDrawer::recreateBuffers(std::vector<std::shared_ptr<TextBlock>> textBlocks) {
    // Destroy vulkan buffers
    this->_destroyBuffer(this->_lineSegmentsIndexBuffer, this->_lineSegmentsIndexBufferMemory);
    this->_destroyBuffer(this->_curveSegmentsIndexBuffer, this->_curveSegmentsIndexBufferMemory);
    this->_destroyBuffer(this->_vertexBuffer, this->_vertexBufferMemory);

    // Create vulkan buffers
    this->_createVertexAndIndexBuffers(textBlocks);
}

void CombinedDrawer::setViewportSize(unsigned int width, unsigned int height) {
    this->_viewportWidth = width;
    this->_viewportHeight = height;
}

void CombinedDrawer::_createVertexAndIndexBuffers(std::vector<std::shared_ptr<TextBlock>> &textBlocks) {
    this->_vertices.clear();
    this->_lineSegmentsIndices.clear();
    this->_curveSegmentsIndices.clear();
    this->_offsets.clear();

    uint32_t vertexCount = 0;
    uint32_t lineSegmentsIndexCount = 0;
    uint32_t curveSegmentsIndexCount = 0;

    for (int i = 0; i < textBlocks.size(); i++) {
        for (Character &character : textBlocks[i]->getCharacters()) {
            GlyphKey key{textBlocks[i]->getFont()->getFontFamily(), character.getUnicodeCodePoint(), 0};
            if (!this->_offsets.contains(key)) {
                this->_offsets.insert({key, {lineSegmentsIndexCount, curveSegmentsIndexCount}});

                this->_vertices.insert(this->_vertices.end(), character.glyph.mesh.getVertices().begin(),
                                       character.glyph.mesh.getVertices().end());
                this->_lineSegmentsIndices.insert(
                    this->_lineSegmentsIndices.end(),
                    character.glyph.mesh.getIndices(GLYPH_MESH_TRIANGLE_BUFFER_INDEX).begin(),
                    character.glyph.mesh.getIndices(GLYPH_MESH_TRIANGLE_BUFFER_INDEX).end());
                this->_curveSegmentsIndices.insert(
                    this->_curveSegmentsIndices.end(),
                    character.glyph.mesh.getIndices(GLYPH_MESH_CURVE_BUFFER_INDEX).begin(),
                    character.glyph.mesh.getIndices(GLYPH_MESH_CURVE_BUFFER_INDEX).end());

                // Add an offset to line segment indices of current character
                for (int j = lineSegmentsIndexCount; j < this->_lineSegmentsIndices.size(); j++) {
                    this->_lineSegmentsIndices.at(j) += vertexCount;
                }

                // Add an offset to curve segment indices of current character
                for (int j = curveSegmentsIndexCount; j < this->_curveSegmentsIndices.size(); j++) {
                    this->_curveSegmentsIndices.at(j) += vertexCount;
                }

                vertexCount += character.glyph.mesh.getVertexCount();
                lineSegmentsIndexCount += character.glyph.mesh.getIndexCount(GLYPH_MESH_TRIANGLE_BUFFER_INDEX);
                curveSegmentsIndexCount += character.glyph.mesh.getIndexCount(GLYPH_MESH_CURVE_BUFFER_INDEX);
            }
        }
    }

    // Check if there are characters to render
    if (vertexCount == 0) {
        return;
    }

    // Create vertex buffer
    VkDeviceSize bufferSize = sizeof(this->_vertices.at(0)) * this->_vertices.size();
    this->_stageAndCreateVulkanBuffer(this->_vertices.data(), bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                      this->_vertexBuffer, this->_vertexBufferMemory);

    // Check if at least one line segment exists
    if (lineSegmentsIndexCount > 0) {
        // Create index buffer for line segments
        VkDeviceSize bufferSize = sizeof(this->_lineSegmentsIndices.at(0)) * this->_lineSegmentsIndices.size();
        this->_stageAndCreateVulkanBuffer(this->_lineSegmentsIndices.data(), bufferSize,
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, this->_lineSegmentsIndexBuffer,
                                          this->_lineSegmentsIndexBufferMemory);
    }

    // Check if at least one curve segment exists
    if (curveSegmentsIndexCount > 0) {
        // Create index buffer for curve segments
        VkDeviceSize bufferSize = sizeof(this->_curveSegmentsIndices.at(0)) * this->_curveSegmentsIndices.size();
        this->_stageAndCreateVulkanBuffer(this->_curveSegmentsIndices.data(), bufferSize,
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, this->_curveSegmentsIndexBuffer,
                                          this->_curveSegmentsIndexBufferMemory);
    }
}

void CombinedDrawer::_createLineSegmentsPipeline() {
    std::vector<char> vertexShaderCode = this->_readFile("shaders/triangle-vert.spv");
    std::vector<char> fragmentShaderCode = this->_readFile("shaders/triangle-frag.spv");

    VkShaderModule vertexShaderModule = this->_createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = this->_createShaderModule(fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo};

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    VkVertexInputBindingDescription vertexInputBindingDescription = vft::getVertexInutBindingDescription();
    VkVertexInputAttributeDescription vertexInputAttributeDescription = vft::getVertexInputAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.size = sizeof(vft::CharacterPushConstants);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &this->_uboDescriptorSetLayout;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    if (vkCreatePipelineLayout(this->_vulkanContext.logicalDevice, &pipelineLayoutCreateInfo, nullptr,
                               &this->_lineSegmentsPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Error creating vulkan pipeline layout");
    }

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.stageCount = 2;
    graphicsPipelineCreateInfo.pStages = shaderStages;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout = this->_lineSegmentsPipelineLayout;
    graphicsPipelineCreateInfo.renderPass = this->_vulkanContext.renderPass;
    graphicsPipelineCreateInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(this->_vulkanContext.logicalDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr,
                                  &this->_lineSegmentsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Error creating vulkan graphics pipeline");
    }

    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, vertexShaderModule, nullptr);
    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, fragmentShaderModule, nullptr);
}

void CombinedDrawer::_createCurveSegmentsPipeline() {
    std::vector<char> vertexShaderCode = this->_readFile("shaders/curve-vert.spv");
    std::vector<char> tcsCode = this->_readFile("shaders/curve-tesc.spv");
    std::vector<char> tesCode = this->_readFile("shaders/curve-tese.spv");
    std::vector<char> fragmentShaderCode = this->_readFile("shaders/curve-frag.spv");

    VkShaderModule vertexShaderModule = this->_createShaderModule(vertexShaderCode);
    VkShaderModule tcsShaderModule = this->_createShaderModule(tcsCode);
    VkShaderModule tesShaderModule = this->_createShaderModule(tesCode);
    VkShaderModule fragmentShaderModule = this->_createShaderModule(fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageCreateInfo.module = vertexShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo tcsShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    vertexShaderStageCreateInfo.module = tcsShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo tesShaderStageCreateInfo{};
    vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    vertexShaderStageCreateInfo.module = tesShaderModule;
    vertexShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo{};
    fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageCreateInfo.module = fragmentShaderModule;
    fragmentShaderStageCreateInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[4] = {};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    shaderStages[1].module = tcsShaderModule;
    shaderStages[1].pName = "main";

    shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    shaderStages[2].module = tesShaderModule;
    shaderStages[2].pName = "main";

    shaderStages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[3].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[3].module = fragmentShaderModule;
    shaderStages[3].pName = "main";

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

    VkVertexInputBindingDescription vertexInputBindingDescription = vft::getVertexInutBindingDescription();
    VkVertexInputAttributeDescription vertexInputAttributeDescription = vft::getVertexInputAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
    vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 1;
    vertexInputStateCreateInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessellationStateCreateInfo{};
    tessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellationStateCreateInfo.patchControlPoints = 3;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    std::array<VkPushConstantRange, 2> pushConstantRanges;

    pushConstantRanges[0].size = sizeof(vft::CharacterPushConstants);
    pushConstantRanges[0].offset = 0;
    pushConstantRanges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    pushConstantRanges[1].size = sizeof(ViewportPushConstants);
    pushConstantRanges[1].offset = sizeof(vft::CharacterPushConstants);
    pushConstantRanges[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &this->_uboDescriptorSetLayout;
    pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = pushConstantRanges.size();

    if (vkCreatePipelineLayout(this->_vulkanContext.logicalDevice, &pipelineLayoutCreateInfo, nullptr,
                               &this->_curveSegmentsPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Error creating vulkan pipeline layout");
    }

    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.stageCount = 4;
    graphicsPipelineCreateInfo.pStages = shaderStages;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    graphicsPipelineCreateInfo.pTessellationState = &tessellationStateCreateInfo;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = nullptr;
    graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout = this->_curveSegmentsPipelineLayout;
    graphicsPipelineCreateInfo.renderPass = this->_vulkanContext.renderPass;
    graphicsPipelineCreateInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(this->_vulkanContext.logicalDevice, nullptr, 1, &graphicsPipelineCreateInfo, nullptr,
                                  &this->_curveSegmentsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Error creating vulkan graphics pipeline");
    }

    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, vertexShaderModule, nullptr);
    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, tcsShaderModule, nullptr);
    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, tesShaderModule, nullptr);
    vkDestroyShaderModule(this->_vulkanContext.logicalDevice, fragmentShaderModule, nullptr);
}

}  // namespace vft
