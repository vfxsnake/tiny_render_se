#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <memory>

// forward declaration
class VulkanContext;
class SwapChain;
class GraphicsPipeline;

class DisplayPipeline
{
public:
    DisplayPipeline(VulkanContext& context, SwapChain& swap_chain);
    
    ~DisplayPipeline();

    void drawFrame();

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

    VulkanContext& context_;
    SwapChain& swapChain_;

    std::unique_ptr<GraphicsPipeline> graphicsPipeline_;

    // Command pool and buffers most be created in this order
    vk::raii::CommandPool commandPool_ = nullptr;
    vk::raii::CommandBuffer commandBuffer_ = nullptr;
    
    vk::raii::Semaphore imageAvailableSemaphore_ = nullptr;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores_;
    
    vk::raii::Fence inFlightFence_ = nullptr;

};