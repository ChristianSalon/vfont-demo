/**
 * @file text_renderer.cpp
 * @author Christian Saloň
 */

#include "text_renderer.h"

namespace vft {

/**
 * @brief TextRenderer constructor
 */
TextRenderer::TextRenderer() {}

/**
 * @brief Text renderer destructor
 */
TextRenderer::~TextRenderer() {
    this->destroy();
}

/**
 * @brief Initializes text renderer
 *
 * @param physicalDevice Vulkan physical device
 * @param logicalDevice Vulkan logical device
 * @param commandPool Vulkan command pool
 * @param graphicsQueue Vulkan graphics queue
 */
void TextRenderer::init(TessellationStrategy tessellationStrategy, VulkanContext vulkanContext) {
    this->_tessellationStrategy = tessellationStrategy;

    this->_setPhysicalDevice(vulkanContext.physicalDevice);
    this->_setLogicalDevice(vulkanContext.logicalDevice);
    this->_setCommandPool(vulkanContext.commandPool);
    this->_setGraphicsQueue(vulkanContext.graphicsQueue);
    this->_setRenderPass(vulkanContext.renderPass);

    if (tessellationStrategy == TessellationStrategy::CPU_ONLY) {
        this->_tessellator = std::make_shared<CpuTessellator>(this->_cache);
        this->_drawer = std::make_shared<CpuDrawer>(this->_cache);
    } else if (tessellationStrategy == TessellationStrategy::GPU_ONLY) {
        this->_tessellator = std::make_shared<GpuTessellator>(this->_cache);
        this->_drawer = std::make_shared<GpuDrawer>(this->_cache);
    } else if (tessellationStrategy == TessellationStrategy::CPU_AND_GPU) {
        this->_tessellator = std::make_shared<CombinedTessellator>(this->_cache);
        this->_drawer = std::make_shared<CombinedDrawer>(this->_cache);
    } else {
        throw std::runtime_error("Invalid tessellation strategy.");
    }

    this->_drawer->init(vulkanContext);
}

/**
 * @brief Must be called to destroy vulkan buffers
 */
void TextRenderer::destroy() {
    this->_tessellator.reset();
    this->_drawer.reset();
}

/**
 * @brief Add vulkan draw commands to selected command buffer
 *
 * @param commandBuffer Selected vulkan command buffer
 */
void TextRenderer::draw(VkCommandBuffer commandBuffer) {
    this->_drawer->draw(this->_blocks, commandBuffer);
}

/**
 * @brief Add text block for rendering
 *
 * @param text Text block to render
 */
void TextRenderer::add(std::shared_ptr<TextBlock> text) {
    text->setTessellationStrategy(this->_tessellator);
    this->_blocks.push_back(text);

    text->onTextChange = [this]() { this->_drawer->recreateBuffers(this->_blocks); };
}

void TextRenderer::setUniformBuffers(vft::UniformBufferObject ubo) {
    this->_drawer->setUniformBuffers(ubo);
}

void TextRenderer::setViewportSize(unsigned int width, unsigned int height) {
    if (this->_tessellationStrategy == TessellationStrategy::CPU_AND_GPU) {
        reinterpret_cast<CombinedDrawer *>(this->_drawer.get())->setViewportSize(width, height);
    } else if (this->_tessellationStrategy == TessellationStrategy::GPU_ONLY) {
        reinterpret_cast<GpuDrawer *>(this->_drawer.get())->setViewportSize(width, height);
    }
}

void TextRenderer::setCache(GlyphCache &cache) {
    this->_cache = cache;
}

void TextRenderer::setCacheSize(unsigned long maxSize) {
    this->_cache.setMaxSize(maxSize);
}

VulkanContext TextRenderer::getVulkanContext() {
    return this->_vulkanContext;
}

/**
 * @brief Setter for vulkan physical device
 *
 * @param physicalDevice Vulkan physical device
 */
void TextRenderer::_setPhysicalDevice(VkPhysicalDevice physicalDevice) {
    if (physicalDevice == nullptr)
        throw std::runtime_error("Vulkan physical device is not initialized");

    this->_vulkanContext.physicalDevice = physicalDevice;
}

/**
 * @brief Setter for vulkan logical device
 *
 * @param logicalDevice Vulkan logical device
 */
void TextRenderer::_setLogicalDevice(VkDevice logicalDevice) {
    if (logicalDevice == nullptr)
        throw std::runtime_error("Vulkan logical device is not initialized");

    this->_vulkanContext.logicalDevice = logicalDevice;
}

/**
 * @brief Setter for vulkan command pool
 *
 * @param commandPool Vulkan command pool
 */
void TextRenderer::_setCommandPool(VkCommandPool commandPool) {
    if (commandPool == nullptr)
        throw std::runtime_error("Vulkan command pool is not initialized");

    this->_vulkanContext.commandPool = commandPool;
}

/**
 * @brief Setter for vulkan graphics queue
 *
 * @param graphicsQueue Vulkan graphics queue
 */
void TextRenderer::_setGraphicsQueue(VkQueue graphicsQueue) {
    if (graphicsQueue == nullptr)
        throw std::runtime_error("Vulkan graphics queue is not initialized");

    this->_vulkanContext.graphicsQueue = graphicsQueue;
}

void TextRenderer::_setRenderPass(VkRenderPass renderPass) {
    if (renderPass == nullptr)
        throw std::runtime_error("Vulkan render pass is not initialized");

    this->_vulkanContext.renderPass = renderPass;
}

}  // namespace vft
