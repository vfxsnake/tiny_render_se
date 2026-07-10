#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <memory>
#include <cstdint>

// forward declaration
class VulkanContext;
class SwapChain;
class GraphicsPipeline;
class Framebuffer;

class DisplayPipeline
{
public:
    DisplayPipeline(
        VulkanContext& context, 
        SwapChain& swap_chain,
        uint32_t frame_buffer_width,
        uint32_t frame_buffer_height
    );
    
    ~DisplayPipeline();

    void drawFrame(const Framebuffer& framebuffer);

private:

    void createCommandPool();
    void createCommandBuffer();
    void createSyncObjects();

    void transitionImageLayout(
        vk::Image image,
        vk::ImageLayout old_layout,
        vk::ImageLayout new_layout,
        vk::PipelineStageFlags2 source_stage_mask,
        vk::AccessFlags2 source_access_mask,
        vk::PipelineStageFlags2 destination_stage_mask,
        vk::AccessFlags2 destination_access_mask
    );

    uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;
    void createTexture();

    void createSampler();
    void createStagingBuffer();
    void createDescriptors();


    VulkanContext& context_;
    SwapChain& swapChain_;

    uint32_t textureWidth_ = 0;
    uint32_t textureHeight_ = 0;

    vk::raii::DeviceMemory textureMemory_ = nullptr;
    vk::raii::Image textureImage_ = nullptr;
    vk::raii::ImageView textureImageView_ = nullptr;
    vk::raii::Sampler sampler_ = nullptr;

    vk::raii::DeviceMemory stagingMemory_ = nullptr;
    vk::raii::Buffer stagingBuffer_ = nullptr;
    void* stagingBufferMemoryMapped_ = nullptr;

    vk::raii::DescriptorSetLayout descriptorSetLayout_ = nullptr;
    vk::raii::DescriptorPool descriptorPool_ = nullptr;
    vk::raii::DescriptorSet descriptorSet_ = nullptr;

    std::unique_ptr<GraphicsPipeline> graphicsPipeline_;

    // Command pool and buffers most be created in this order
    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::CommandBuffer commandBuffer_ = nullptr;
    
    vk::raii::Semaphore imageAvailableSemaphore_ = nullptr;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    
    vk::raii::Fence inFlightFence_ = nullptr;

};