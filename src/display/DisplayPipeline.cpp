#include "DisplayPipeline.h"

#include <vector>
#include <array>
#include <cstring>

#include "core/VulkanContext.h"
#include "core/SwapChain.h"
#include "GraphicsPipeline.h"
#include "rasterizer/Framebuffer.h"

DisplayPipeline::DisplayPipeline(
    VulkanContext& context, 
    SwapChain& swap_chain,
    uint32_t frame_buffer_width,
    uint32_t frame_buffer_height
):
    context_(context),
    swapChain_(swap_chain),
    textureWidth_(frame_buffer_width),
    textureHeight_(frame_buffer_height)
{
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
    createTexture();
    createSampler();
    createStagingBuffer();
    createDescriptors();
    graphicsPipeline_ = std::make_unique<GraphicsPipeline>(
        context_, 
        swapChain_.getFormat(),
        *descriptorSetLayout_
    );
}

DisplayPipeline::~DisplayPipeline() = default;

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


void DisplayPipeline::drawFrame(const Framebuffer& framebuffer)
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

    std::memcpy(
        stagingBufferMemoryMapped_,
        framebuffer.getData(),
        static_cast<size_t>(textureWidth_) * textureHeight_ * 4
    );

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

    // texture barrier
    transitionImageLayout(
        *textureImage_,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        {},
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite
    );

    vk::BufferImageCopy copy_region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0,0,0},
        .imageExtent = {textureWidth_, textureHeight_, 1}
    };

    commandBuffer_.copyBufferToImage(
        *stagingBuffer_,
        *textureImage_,
        vk::ImageLayout::eTransferDstOptimal,
        copy_region
    );

    transitionImageLayout(
        *textureImage_,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::AccessFlagBits2::eShaderRead
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
    vk::Extent2D extent = swapChain_.getExtent();
    vk::Viewport view_port{
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vk::Rect2D scissors {
        .offset = {0, 0},
        .extent = extent
    };
    commandBuffer_.bindPipeline(
        vk::PipelineBindPoint::eGraphics,
        *(graphicsPipeline_->getPipeline())
    );

    commandBuffer_.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *graphicsPipeline_->getPipelineLayout(),
        0,
        *descriptorSet_,
        nullptr
    );

    commandBuffer_.setViewport(0, view_port);
    commandBuffer_.setScissor(0, scissors);

    commandBuffer_.draw(3, 1, 0, 0);

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


uint32_t DisplayPipeline::findMemoryType(
    uint32_t type_filter, 
    vk::MemoryPropertyFlags properties
) const
{
    vk::PhysicalDeviceMemoryProperties memory_properties = context_.getPhysicalDevice().getMemoryProperties();
    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if (
            (type_filter & (1 << i)) && 
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties
        )
        {
            return i;
        }
    }

    throw std::runtime_error("faliled to find suitable memory type!");
}


void DisplayPipeline::createTexture()
{
    vk::ImageCreateInfo image_create_info{
        .imageType = vk::ImageType::e2D,
        .format = vk::Format::eR8G8B8A8Unorm,
        .extent = {.width = textureWidth_, .height = textureHeight_, .depth = 1}, // vk:Extent
        .mipLevels = 1,
        .arrayLayers = 1,
        .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive
    };

    textureImage_ = vk::raii::Image(context_.getLogicalDevice(), image_create_info);
    
    vk::MemoryRequirements memory_requirements = textureImage_.getMemoryRequirements();
    
    uint32_t memory_type = findMemoryType(
        memory_requirements.memoryTypeBits, 
        vk::MemoryPropertyFlagBits::eDeviceLocal
    );
    
    vk::MemoryAllocateInfo memory_allocate_info{
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type
    };

    textureMemory_ = vk::raii::DeviceMemory(
        context_.getLogicalDevice(),
        memory_allocate_info
    );

    textureImage_.bindMemory(*textureMemory_, 0);

    vk::ImageViewCreateInfo image_view_create_info{
        .image = *textureImage_,
        .viewType = vk::ImageViewType::e2D,
        .format = vk::Format::eR8G8B8A8Unorm,
        .subresourceRange = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    textureImageView_ = vk::raii::ImageView(context_.getLogicalDevice(), image_view_create_info);
}


void DisplayPipeline::createSampler()
{
    vk::SamplerCreateInfo sampler_create_info{
        .magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
        .maxLod = 0.0f
    };

    sampler_ = vk::raii::Sampler(context_.getLogicalDevice(), sampler_create_info);
}


void DisplayPipeline::createStagingBuffer()
{
    // vk::DeviceSize(textureWidth_) build up the arithmetic to 64bits; 4 is for RGBA values
    vk::DeviceSize buffer_size = vk::DeviceSize(textureWidth_) * textureHeight_ * 4;
    
    vk::BufferCreateInfo buffer_create_info{
        .size = buffer_size,
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive
    };

    stagingBuffer_ = vk::raii::Buffer(context_.getLogicalDevice(), buffer_create_info);

    vk::MemoryRequirements memory_requirements = stagingBuffer_.getMemoryRequirements();

    uint32_t memory_type =  findMemoryType(
        memory_requirements.memoryTypeBits,
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent 
    );

    vk::MemoryAllocateInfo stagin_memory_allocate_info{
        .allocationSize = memory_requirements.size,
        .memoryTypeIndex = memory_type
    };

    stagingMemory_ = vk::raii::DeviceMemory(
        context_.getLogicalDevice(),
        stagin_memory_allocate_info
    );

    stagingBuffer_.bindMemory(*stagingMemory_, 0);
    stagingBufferMemoryMapped_ = stagingMemory_.mapMemory(0, buffer_size);
}


void DisplayPipeline::createDescriptors()
{
    // creating DescriptorSetLayout
    vk::DescriptorSetLayoutBinding descriptor_set_layout_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr
    };

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .bindingCount = 1,
        .pBindings = &descriptor_set_layout_binding
    };

    descriptorSetLayout_ = vk::raii::DescriptorSetLayout(
        context_.getLogicalDevice(),
        descriptor_set_layout_create_info
    );

    // creating DescriptorSetPool
    vk::DescriptorPoolSize descriptor_pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1
    };
    vk::DescriptorPoolCreateInfo descriptor_pool_create_info{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &descriptor_pool_size,
    };

    descriptorPool_ = vk::raii::DescriptorPool(
        context_.getLogicalDevice(),
        descriptor_pool_create_info
    );

    // allocate set
    vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{
        .descriptorPool = *descriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &*descriptorSetLayout_
    };

    std::vector<vk::raii::DescriptorSet> descriptor_sets = context_.getLogicalDevice().allocateDescriptorSets(descriptor_set_allocate_info);
    descriptorSet_ = std::move(descriptor_sets.front());

    // writing the descriptor
    vk::DescriptorImageInfo descriptor_image_info{
        .sampler = *sampler_,
        .imageView = *textureImageView_,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    vk::WriteDescriptorSet write_descriptor_set{
        .dstSet = *descriptorSet_,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .pImageInfo = &descriptor_image_info
    };

    context_.getLogicalDevice().updateDescriptorSets({write_descriptor_set}, {});
}