#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>

#include "rasterizer/Framebuffer.h"

// forward declarations
class VulkanContext;
class SwapChain;
class DisplayPipeline;

class Application
{
public:
    Application();
    ~Application();

    void run();
    
private:
    void initWindow();
    void mainLoop();

    void initVulkan();

    void drawTextPattern();

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;

    Framebuffer framebuffer_;

    // vulkan unique pointer objects
    std::unique_ptr<VulkanContext> context_;
    std::unique_ptr<SwapChain> swapChain_;
    std::unique_ptr<DisplayPipeline> displayPipeline_;
};