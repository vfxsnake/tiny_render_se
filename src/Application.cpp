#include "Application.h"

#include <stdexcept>

#include <core/VulkanContext.h>
#include <core/SwapChain.h>

#include "display/DisplayPipeline.h"
#include "rasterizer/Color.h"

Application::Application() :
    framebuffer_(WIDTH, HEIGHT)
{
    initWindow();
    initVulkan();
}


Application::~Application()
{
    context_->getLogicalDevice().waitIdle();
    glfwDestroyWindow(window_);
    glfwTerminate();
}


void Application::run()
{
    drawTextPattern();
    mainLoop();
}


void Application::initWindow()
{
    if (!glfwInit())
    {
        throw std::runtime_error("Unable to initialize GlFW");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // the size is going to be constant for now.

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Tiny Renderer SE", nullptr, nullptr);
    if (!window_)
    {
        throw std::runtime_error("unable to create GLFW window");
    }
}


void Application::initVulkan()
{
    context_ = std::make_unique<VulkanContext>(window_);
    swapChain_ = std::make_unique<SwapChain>(*context_, window_);
    displayPipeline_ = std::make_unique<DisplayPipeline>(
        *context_, 
        *swapChain_,
        WIDTH,
        HEIGHT
    );
}


void Application::mainLoop()
{
    while (!glfwWindowShouldClose(window_))
    {
        glfwPollEvents();
        displayPipeline_->drawFrame(framebuffer_);
    }
}


void Application::drawTextPattern()
{
    for (int y = 0; y < framebuffer_.getHeight(); y++)
    {
        for (int x = 0; x < framebuffer_.getWidth(); x++)
        {
            Color color{
                .r = static_cast<uint8_t>(255* x/ (framebuffer_.getWidth() -1)),
                .g = static_cast<uint8_t>(255* x/ (framebuffer_.getWidth() -1)),
                .b = 0,
                .a = 255
            };
            framebuffer_.setPixel(x, y, color);
        }
    }
}