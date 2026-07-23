#include "Application.h"

#include <stdexcept>

#include <core/VulkanContext.h>
#include <core/SwapChain.h>
#include <random>
#include <vector>
#include <array>
#include <iostream>

#include "display/DisplayPipeline.h"
#include "rasterizer/Color.h"
#include "rasterizer/LineDrawer.h"
#include "rasterizer/TriangleRasterizer.h"
#include "utils/Timer.h"



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

    Color black{0,0,0,0};
    Color white{255, 255, 255, 255};
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};
    Color blue{0, 0, 255, 255};

    // algorithms test:
    // testDrawLineAlgorithms();
    testDrawTriangleAlgorithms();

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

void Application::testDrawLineAlgorithms()
{    
    // initializing Mersenne twister engine with the static seed
    std::mt19937 gen(147);

    // defining the range
    std::uniform_int_distribution<int> dist_x{0, WIDTH - 1};
    std::uniform_int_distribution<int> dist_y{0, HEIGHT - 1};

    // constructing a list of valid lines
    std::vector<std::array<Vec2i, 2>> line_list;
    int line_count = 0;
    while (line_count < 100000)
    {
        Vec2i a = {dist_x(gen), dist_y(gen)};
        Vec2i b = {dist_x(gen), dist_y(gen)};

        if (std::abs(b.x - a.x) >= 200 || std::abs(b.y - a.y) >= 200)
        {
            line_list.push_back({a, b});
            line_count ++;
        }
    }

    Color black{0, 0, 0, 0};
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0,  255};
    Color blue{0, 0, 255, 255};
    
    framebuffer_.clear(black);
    // draw line Naive draw
    Timer timer;
    timer.start();
    for (auto& start_end_points : line_list)
    {
        LineDrawer::drawLineNaive(start_end_points[0], start_end_points[1], red, framebuffer_);
    }
    timer.stop();
    std::cout << "drawLineNaive processing time: " << timer.elapsedMs() << "ms.\n";


    timer.start();
    for (auto& start_end_points : line_list)
    {
        LineDrawer::drawLineAccum(start_end_points[0], start_end_points[1], green, framebuffer_);
    }
    timer.stop();
    std::cout << "drawLineAccum processing time: " << timer.elapsedMs() << "ms.\n";

    timer.start();
    for (auto& start_end_points : line_list)
    {

        LineDrawer::drawLine(start_end_points[0], start_end_points[1], blue, framebuffer_);
    }
    timer.stop();
    std::cout << "drawLine Bresenham processing time: " << timer.elapsedMs() << "ms.\n";
}


void Application::testDrawTriangleAlgorithms()
{
    // generate triangles
    // initializing Mersenne twister engine with the static seed
    std::mt19937 gen(1313);

    // defining the range
    std::uniform_int_distribution<int> dist_x{0, WIDTH - 1};
    std::uniform_int_distribution<int> dist_y{0, HEIGHT - 1};
    Color white{255, 255, 255, 255};
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};

    int triangle_count = 0;
    std::vector<std::array<Vec2i,3>> points_array_list;
    while (triangle_count < 1000)
    {
        Vec2i a = {dist_x(gen), dist_y(gen)};
        Vec2i b = {dist_x(gen), dist_y(gen)};
        Vec2i c = {dist_x(gen), dist_y(gen)};

        std::array<Vec2i,3> triangle_points = {a, b, c};
        int min_x = std::min({a.x, b.x, c.x});
        int max_x = std::max({a.x, b.x, c.x});
        int min_y = std::min({a.y, b.y, c.y});
        int max_y = std::max({a.y, b.y, c.y});
        
        if (max_x -min_x > 400 || max_y - min_y > 400 )
        {
            continue;
        }

        points_array_list.push_back(triangle_points);
        triangle_count += 1;
    }

    Timer timer;

    timer.start();
    for (auto point_array : points_array_list)
    {
        Triangle triangle{point_array[0], point_array[1], point_array[2]};
        TriangleRasterizer::drawTriangleScanline(triangle, red, framebuffer_);
    }
    timer.stop();
    std::cout << "drawTriangle rasterize processing time: " << timer.elapsedMs() << "ms.\n";
    
    timer.start();
    for (auto point_array : points_array_list)
    {
        Triangle triangle{point_array[0], point_array[1], point_array[2]};
        TriangleRasterizer::drawTriangle(triangle, green, framebuffer_);

    }
    timer.stop();
    std::cout << "drawTriangle barycentric processing time: " << timer.elapsedMs() << "ms.\n";
}   