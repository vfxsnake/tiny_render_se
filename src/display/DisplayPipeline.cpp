#include "DisplayPipeline.h"

#include <vector>
#include <array>

#include "core/VulkanContext.h"
#include "core/SwapChain.h"

DisplayPipeline::DisplayPipeline(VulkanContext& context, SwapChain& swap_chain):
    context_(context),
    swapChain_(swap_chain)
{
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
}


void DisplayPipeline::createCommandPool()
{
    vk::CommandPoolCreateInfo command_pool_create_info{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer, // permit the use of .reset method of the command buffer.
        .queueFamilyIndex = context_.getQueueFamilyIndex()
    };

    commandPool_ = vk::raii::CommandPool(context_.getLogicalDevice(), command_pool_create_info);
}


void DisplayPipeline::createCommandBuffer()
{
    vk::CommandBufferAllocateInfo command_buffer_allocate_info{
        .commandPool = *commandPool_,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    // allocateCommandBuffers returns a std::vector<vk::raii::CommandBuffer>
    auto buffers = context_.getLogicalDevice().allocateCommandBuffers(command_buffer_allocate_info);
    commandBuffer_ = std::move(buffers[0]);
}


void DisplayPipeline::createSyncObjects()
{
    imageAvailableSemaphore_ = vk::raii::Semaphore(
        context_.getLogicalDevice(),
        vk::SemaphoreCreateInfo()
    );

    // creating semaphores per swapchain image
    uint32_t image_count = swapChain_.getImageCount();

    for (uint32_t i = 0; i < image_count; i++)
    {
        // constructs a vk::raii::Semaphore in place avoiding any move operation.
        renderFinishedSemaphores_.emplace_back(
            context_.getLogicalDevice(),
            vk::SemaphoreCreateInfo()
        );
    }

    vk::FenceCreateInfo fence_create_info{
        .flags = vk::FenceCreateFlagBits::eSignaled  
    };

    inFlightFence_ = vk::raii::Fence(
        context_.getLogicalDevice(),
        fence_create_info
    );
}


void DisplayPipeline::drawFrame()
{
    vk::Result fence_result = context_.getLogicalDevice().waitForFences(
        *inFlightFence_,
        vk::True,
        UINT64_MAX
    );

    if (fence_result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Fence result was not success, failing on waiting for fence!");
    }

    // reseting cpu fence to wait for the gpu.
    context_.getLogicalDevice().resetFences(*inFlightFence_);

    [[maybe_unused]] auto [available_image_result, image_index] = swapChain_.get().acquireNextImage(
        UINT64_MAX,
        *imageAvailableSemaphore_,
        nullptr
    );

    // command buffer reset
    commandBuffer_.reset();

    vk::CommandBufferBeginInfo command_buffer_begin_info{};
    commandBuffer_.begin(command_buffer_begin_info);
    
    // call for transitionImageLayout
    transitionImageLayout(
        swapChain_.getImage(image_index),
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite
    );

    // preparing rendering attachment info
    vk::ClearValue clear_value{
        .color = { // vk::CleanColorValue struct. using the float initializer
            .float32 = std::array<float,4>{1.0f, 0.0f, 0.0f, 1.0f}
        }
    };

    vk::RenderingAttachmentInfo attachment_info{
        .imageView = *swapChain_.getImageView(image_index),
        .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .clearValue = clear_value
    };

    vk::RenderingInfo rendering_info{
        .renderArea{
            .offset = {0,0},
            .extent = swapChain_.getExtent()
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment_info
    };

    commandBuffer_.beginRendering(rendering_info);
    commandBuffer_.endRendering();

    transitionImageLayout(
        swapChain_.getImage(image_index),
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        {}
    );

    commandBuffer_.end();

    vk::PipelineStageFlags pipeline_flags = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submit_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*imageAvailableSemaphore_,
        .pWaitDstStageMask = &pipeline_flags,
        .commandBufferCount = 1,
        .pCommandBuffers = &*commandBuffer_,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*renderFinishedSemaphores_[image_index]
    };

    context_.getQueue().submit(submit_info, *inFlightFence_);

    // presenter
    const vk::PresentInfoKHR present_info{
        .waitSemaphoreCount =1,
        .pWaitSemaphores = &*renderFinishedSemaphores_[image_index],
        .swapchainCount = 1,
        .pSwapchains = &*swapChain_.get(),
        .pImageIndices = &image_index
    };

    // as presenterKHR is nodiscard value casting to void to avoid flagging errors
    static_cast<void>(context_.getQueue().presentKHR(present_info));
}


void DisplayPipeline::transitionImageLayout(
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::PipelineStageFlags2 source_stage_mask,
    vk::AccessFlags2 source_access_mask,
    vk::PipelineStageFlags2 destination_stage_mask,
    vk::AccessFlags2 destination_access_mask
)
{
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask = source_stage_mask,
        .srcAccessMask = source_access_mask,
        .dstStageMask = destination_stage_mask,
        .dstAccessMask = destination_access_mask,
        .oldLayout = old_layout,
        .newLayout = new_layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    vk::DependencyInfo dependency_info{
        .dependencyFlags = {},
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier
    };

    commandBuffer_.pipelineBarrier2(dependency_info);
}