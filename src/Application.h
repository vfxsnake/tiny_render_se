#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <memory>

#include "rasterizer/Framebuffer.h"
#include "math/Vec2.h"
#include "rasterizer/Color.h"

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

    void drawPoint()
    {
        Color black{0,0,0,0};
        Color white{255, 255, 255, 255};
        
        framebuffer_.clear(black);

        tinymath::Vec2i point_1{100, 100};
        tinymath::Vec2i point_2{700, 100};
        tinymath::Vec2i point_3{100, 500};

        framebuffer_.setPixel(point_1.x, point_1.y, white);
        framebuffer_.setPixel(point_2.x, point_2.y, white);
        framebuffer_.setPixel(point_3.x, point_3.y, white);

    }

    // testing helpers
    void testDrawLineAlgorithms();
    void testDrawTriangleAlgorithms();
    void testDrawWireFrame();
    void testDrawMesh();
    void testDrawMeshMatrix();
    void testDrawMeshMatrixLightWorldSpace();
    void testDrawMeshMatrixLightScreenSpace();
    void testDrawMeshMatrixLightNormalsCheckIntegrity();
    void testDrawMeshRandomShader();

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 800;
    GLFWwindow* window_ = nullptr;

    Framebuffer framebuffer_;

    // vulkan unique pointer objects
    std::unique_ptr<VulkanContext> context_;
    std::unique_ptr<SwapChain> swapChain_;
    std::unique_ptr<DisplayPipeline> displayPipeline_;
};