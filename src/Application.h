#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>

// forward declarations
class VulkanContext;
class SwapChain;

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

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;

    // vulkan unique pointer objects
    std::unique_ptr<VulkanContext> context_;
    std::unique_ptr<SwapChain> swapChain_;
};