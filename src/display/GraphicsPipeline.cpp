#include "GraphicsPipeline.h"

#include <vector>

#include "core/VulkanContext.h"
#include "utils/FileUtils.h"


GraphicsPipeline::GraphicsPipeline(const VulkanContext& context, vk::Format color_format):
    context_(context)
{
    createPipelineLayout();
    createPipeline(color_format);
}


void GraphicsPipeline::createPipelineLayout()
{
    vk::PipelineLayoutCreateInfo pipeline_layout_create_info{};

    pipelineLayout_ = context_.getLogicalDevice().createPipelineLayout(pipeline_layout_create_info);
}


vk::raii::ShaderModule GraphicsPipeline::createShaderModule(const std::string& spirv_path) const
{
    std::vector<uint32_t> shader_code = readSpirv(spirv_path);
    vk::ShaderModuleCreateInfo shader_module_create_info{
        .codeSize = shader_code.size() * sizeof(uint32_t),
        .pCode = shader_code.data()
    };

    return vk::raii::ShaderModule(context_.getLogicalDevice(), shader_module_create_info);
}

void GraphicsPipeline::createPipeline(vk::Format color_format)
{
    vk::raii::ShaderModule shader_module = createShaderModule("shaders/display.spv");

    vk::PipelineShaderStageCreateInfo vertex_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shader_module,
        .pName = "vertMain"
    };
    vk::PipelineShaderStageCreateInfo fragment_shader_stage_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shader_module,
        .pName = "fragMain"
    };

    vk::PipelineShaderStageCreateInfo shader_stages[] = {vertex_shader_stage_info, fragment_shader_stage_info};

    vk::PipelineVertexInputStateCreateInfo vertex_input_info{};
  
    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleList
    };

    std::vector<vk::DynamicState> dynamic_states = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamic_state_create_info{
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    vk::PipelineViewportStateCreateInfo viewport_state{
        .viewportCount = 1,
        .scissorCount = 1,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f
    };
    
    vk::PipelineMultisampleStateCreateInfo multi_sampling_state_create_info{
        .rasterizationSamples = vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = vk::False
    };

    vk::PipelineColorBlendAttachmentState color_blend_attachment_state{
        .blendEnable = vk::False,
        .colorWriteMask = vk::ColorComponentFlagBits::eR | 
                          vk::ColorComponentFlagBits::eG | 
                          vk::ColorComponentFlagBits::eB | 
                          vk::ColorComponentFlagBits::eA 
    };

    vk::PipelineColorBlendStateCreateInfo color_blending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy, 
        .attachmentCount = 1, 
        .pAttachments = &color_blend_attachment_state
    };

    vk::GraphicsPipelineCreateInfo graphics_pipeline_create_info{
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multi_sampling_state_create_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state_create_info,
        .layout = *pipelineLayout_,
        .renderPass = nullptr
    };
    vk::PipelineRenderingCreateInfo pipeline_rendering_create_info{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_format
    };

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipeline_create_info_chain = {
        graphics_pipeline_create_info, 
        pipeline_rendering_create_info
    };

    pipeline_ = vk::raii::Pipeline(context_.getLogicalDevice(), nullptr, pipeline_create_info_chain.get<vk::GraphicsPipelineCreateInfo>());
}


vk::raii::Pipeline const& GraphicsPipeline::getPipeline() const
{
    return pipeline_;
}