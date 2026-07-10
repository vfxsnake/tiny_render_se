#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>

// forward declarations
class VulkanContext;


class GraphicsPipeline
{
public:
    GraphicsPipeline(
        const VulkanContext& context, 
        vk::Format color_format,
        vk::DescriptorSetLayout descriptor_set_layout
    );

    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator =(const GraphicsPipeline&) = delete;

    auto getPipeline() const -> vk::raii::Pipeline const&;
    auto getPipelineLayout() const -> vk::raii::PipelineLayout const&;

private:
    void createPipelineLayout(vk::DescriptorSetLayout descriptor_set_layout);
    void createPipeline(vk::Format color_format);

    auto createShaderModule(const std::string& spirv_path) const -> vk::raii::ShaderModule;

    const VulkanContext& context_;
    vk::raii::PipelineLayout pipelineLayout_ = nullptr;
    vk::raii::Pipeline pipeline_ = nullptr;

};