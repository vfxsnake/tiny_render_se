#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>


class Application
{
public:
    void run();
    
private:
    void initWindow();
    void mainLoop();
    void cleanup();

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;
    GLFWwindow* window_ = nullptr;
};