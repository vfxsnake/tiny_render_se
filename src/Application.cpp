#include "Application.h"

#include <stdexcept>

#include <core/VulkanContext.h>
#include <core/SwapChain.h>
#include <random>
#include <vector>
#include <array>
#include <iostream>
#include <algorithm>

#include "display/DisplayPipeline.h"
#include "rasterizer/Color.h"
#include "rasterizer/LineDrawer.h"
#include "rasterizer/TriangleRasterizer.h"
#include "rasterizer/shaders/RandomShader.h"
#include "rasterizer/shaders/FaceShader.h"
#include "rasterizer/shaders/GouraudShader.h"
#include "rasterizer/shaders/LambertShader.h"
#include "rasterizer/shaders/PhongShader.h"
#include "rasterizer/shaders/BlinnPhongShader.h"
#include "rasterizer/shaders/UvColorShader.h"
#include "rasterizer/shaders/TextureShader.h"
#include "rasterizer/shaders/MaterialShader.h"
#include "rasterizer/shaders/DepthShader.h"
#include "rasterizer/Texture.h"
#include "geometry/Mesh.h"
#include "utils/Timer.h"
#include "io/ObjLoader.h"
#include "io/TextureLoader.h"
#include "math/Projection.h"
#include "math/Transform.h"



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

    // algorithms test:
    // testDrawLineAlgorithms();
    // testDrawTriangleAlgorithms();
    // testDrawWireFrame();
    // testDrawMesh();
    // testDrawMeshMatrixLightWorldSpace();
    // testDrawMeshMatrixLightScreenSpace();
    // testDrawMeshMatrixLightNormalsCheckIntegrity();
    // testDrawMeshRandomShader();
    // testDrawMeshFaceShader();
    // testDrawMeshGouraudShader();
    // testDrawMeshLambertShader();
    // testDrawMeshPhongShader();
    // testDrawMeshBlinnPhongShader();
    // testDrawMeshUvColorShader();
    // testDrawMeshTextureShader();
    // testDrawMeshMaterialShader();
    testDrawMeshShadowMap();

    mainLoop();
}


void Application::blitDepthAsGrayscale(const Framebuffer& source)
{
    for (int y = 0; y < source.getHeight(); y++)
    {
        for (int x = 0; x < source.getWidth(); x++)
        {
            float depth_value = source.getDepth(x, y);
            uint8_t depth_grayscale = static_cast<uint8_t>(depth_value * 255);
            framebuffer_.setPixel(x, y, {depth_grayscale, depth_grayscale, depth_grayscale, 255});
        }
    }
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
    std::vector<std::array<tinymath::Vec2i, 2>> line_list;
    int line_count = 0;
    while (line_count < 100000)
    {
        tinymath::Vec2i a = {dist_x(gen), dist_y(gen)};
        tinymath::Vec2i b = {dist_x(gen), dist_y(gen)};

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
    Color red{255, 0, 0, 255};
    Color green{0, 255, 0, 255};

    int triangle_count = 0;
    std::vector<std::array<tinymath::Vec2i,3>> points_array_list;
    while (triangle_count < 1000)
    {
        tinymath::Vec2i a = {dist_x(gen), dist_y(gen)};
        tinymath::Vec2i b = {dist_x(gen), dist_y(gen)};
        tinymath::Vec2i c = {dist_x(gen), dist_y(gen)};

        std::array<tinymath::Vec2i,3> triangle_points = {a, b, c};
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
        Triangle2D triangle{point_array[0], point_array[1], point_array[2]};
        TriangleRasterizer::drawTriangleScanline(triangle, red, framebuffer_);
    }
    timer.stop();
    std::cout << "drawTriangle rasterize processing time: " << timer.elapsedMs() << "ms.\n";
    
    timer.start();
    for (auto point_array : points_array_list)
    {
        Triangle2D triangle{point_array[0], point_array[1], point_array[2]};
        TriangleRasterizer::drawTriangle2D(triangle, green, framebuffer_);

    }
    timer.stop();
    std::cout << "drawTriangle barycentric processing time: " << timer.elapsedMs() << "ms.\n";
}   


void Application::testDrawWireFrame()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    Color red{255, 0,0, 255};
    Color green{0, 255, 0, 255};
    Color blue{0, 0, 255, 255};

    for (const std::array<int, 3>& face_indices : geometry_mesh.faceIndices)
    {
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];

        a = tinymath::orthographicProjection(a, WIDTH, HEIGHT, 1.0f);
        b = tinymath::orthographicProjection(b, WIDTH, HEIGHT, 1.0f);
        c = tinymath::orthographicProjection(c, WIDTH, HEIGHT, 1.0f);
   
        LineDrawer::drawLine(
            {static_cast<int>(a.x), static_cast<int>(a.y)}, 
            {static_cast<int>(b.x), static_cast<int>(b.y)}, 
            red, 
            framebuffer_
        );
        LineDrawer::drawLine(
            {static_cast<int>(b.x), static_cast<int>(b.y)}, 
            {static_cast<int>(c.x), static_cast<int>(c.y)}, 
            green, 
            framebuffer_
        );
        LineDrawer::drawLine(
            {static_cast<int>(c.x), static_cast<int>(c.y)}, 
            {static_cast<int>(a.x), static_cast<int>(a.y)}, 
            blue, 
            framebuffer_
        );       
    }
}

void Application::testDrawMesh()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    // color randomization setup
    std::mt19937 gen(753);
    std::uniform_int_distribution<int> dist{0, 255};
    std::vector<std::pair<Triangle, Color>> triangle_and_color_list;
    for (const std::array<int, 3>& face_indices : geometry_mesh.faceIndices)
    {
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];

        // apply rotation
        a = tinymath::rotateY(a, 0.785f);
        b = tinymath::rotateY(b, 0.785f);
        c = tinymath::rotateY(c, 0.785f);

        // perspective
        a = tinymath::perspectiveZDivide(a, 3);
        b = tinymath::perspectiveZDivide(b, 3);
        c = tinymath::perspectiveZDivide(c, 3);
        

        // apply projection
        tinymath::Vec3f projected_a = tinymath::orthographicProjection(a, WIDTH, HEIGHT, 1.0f);
        tinymath::Vec3f projected_b = tinymath::orthographicProjection(b, WIDTH, HEIGHT, 1.0f);
        tinymath::Vec3f projected_c = tinymath::orthographicProjection(c, WIDTH, HEIGHT, 1.0f);
        
        Triangle triangle{
            {{projected_a.x, projected_a.y}, projected_a.z},
            {{projected_b.x, projected_b.y}, projected_b.z},
            {{projected_c.x, projected_c.y}, projected_c.z},
        };

        
        Color rand_color{
            static_cast<uint8_t>(dist(gen)),
            static_cast<uint8_t>(dist(gen)),
            static_cast<uint8_t>(dist(gen)),
            255 
        };
        
        triangle_and_color_list.push_back({triangle, rand_color});
    }

    Timer timer;
    timer.start();
    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_, false);
    }
    timer.stop();
    std::cout << "drawTriangleSolidColor no back face culling processing time: " << timer.elapsedMs() << "ms.\n";

    timer.start();
    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_);
    }
    timer.stop();
    std::cout << "drawTriangleSolidColor with back face culling processing time: " << timer.elapsedMs() << "ms.\n";
}


void Application::testDrawMeshMatrix()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    // color randomization setup
    std::mt19937 gen(753);
    std::uniform_int_distribution<int> dist{0, 255};
    std::vector<std::pair<Triangle, Color>> triangle_and_color_list;

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::viewport(WIDTH, HEIGHT) * 
                                                tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );

    for (const std::array<int, 3>& face_indices : geometry_mesh.faceIndices)
    {
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];
        
        tinymath::Vec4f a_transform = transformation_matrix * tinymath::toVec4(a);
        tinymath::Vec4f b_transform = transformation_matrix * tinymath::toVec4(b); 
        tinymath::Vec4f c_transform = transformation_matrix * tinymath::toVec4(c); 

        tinymath::Vec3f a_projected = tinymath::toVec3(a_transform);
        tinymath::Vec3f b_projected = tinymath::toVec3(b_transform);
        tinymath::Vec3f c_projected = tinymath::toVec3(c_transform);

        Triangle triangle{
            {{a_projected.x, a_projected.y}, a_projected.z},
            {{b_projected.x, b_projected.y}, b_projected.z},
            {{c_projected.x, c_projected.y}, c_projected.z},
        };

        
        Color rand_color{
            static_cast<uint8_t>(dist(gen)),
            static_cast<uint8_t>(dist(gen)),
            static_cast<uint8_t>(dist(gen)),
            255 
        };
        
        triangle_and_color_list.push_back({triangle, rand_color});
    }

    Timer timer;
    timer.start();
    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_, false);
    }
    timer.stop();
    std::cout << "drawTriangleSolidColor no back face culling processing time: " << timer.elapsedMs() << "ms.\n";

    timer.start();
    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_);
    }
    timer.stop();
    std::cout << "drawTriangleSolidColor with back face culling processing time: " << timer.elapsedMs() << "ms.\n";
}


void Application::testDrawMeshMatrixLightWorldSpace()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    std::vector<std::pair<Triangle, Color>> triangle_and_color_list;

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::viewport(WIDTH, HEIGHT) * 
                                                tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    // temp light direction
    tinymath::Vec3f light_direction = {0.0f, 0.0f, 1.0f};

    for (const std::array<int, 3>& face_indices : geometry_mesh.faceIndices)
    {
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];
        
        // calculate triangle normal
        tinymath::Vec3f triangle_normal = normalize(cross(b - a, c - a));
        
        float intensity = std::max(0.0f, dot(triangle_normal, light_direction));
        
        Color triangle_color{
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            255 
        };

        tinymath::Vec4f a_transform = transformation_matrix * tinymath::toVec4(a);
        tinymath::Vec4f b_transform = transformation_matrix * tinymath::toVec4(b); 
        tinymath::Vec4f c_transform = transformation_matrix * tinymath::toVec4(c); 

        tinymath::Vec3f a_projected = tinymath::toVec3(a_transform);
        tinymath::Vec3f b_projected = tinymath::toVec3(b_transform);
        tinymath::Vec3f c_projected = tinymath::toVec3(c_transform);

        Triangle triangle{
            {{a_projected.x, a_projected.y}, a_projected.z},
            {{b_projected.x, b_projected.y}, b_projected.z},
            {{c_projected.x, c_projected.y}, c_projected.z},
        };
        
        triangle_and_color_list.push_back({triangle, triangle_color});
    }

    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_);
    }

}

void Application::testDrawMeshMatrixLightScreenSpace()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    std::vector<std::pair<Triangle, Color>> triangle_and_color_list;

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::viewport(WIDTH, HEIGHT) * 
                                                tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    // temp light direction
    tinymath::Vec3f light_direction = {0.0f, 0.0f, 1.0f};

    for (const std::array<int, 3>& face_indices : geometry_mesh.faceIndices)
    {
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];
        
        tinymath::Vec4f a_transform = transformation_matrix * tinymath::toVec4(a);
        tinymath::Vec4f b_transform = transformation_matrix * tinymath::toVec4(b); 
        tinymath::Vec4f c_transform = transformation_matrix * tinymath::toVec4(c); 

        tinymath::Vec3f a_projected = tinymath::toVec3(a_transform);
        tinymath::Vec3f b_projected = tinymath::toVec3(b_transform);
        tinymath::Vec3f c_projected = tinymath::toVec3(c_transform);

        // calculate triangle normal
        tinymath::Vec3f triangle_normal = normalize(cross(b_projected - a_projected, c_projected - a_projected));
        
        float intensity = std::max(0.0f, dot(triangle_normal, light_direction));
        
        Color triangle_color{
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            255 
        };

        Triangle triangle{
            {{a_projected.x, a_projected.y}, a_projected.z},
            {{b_projected.x, b_projected.y}, b_projected.z},
            {{c_projected.x, c_projected.y}, c_projected.z},
        };
        
        triangle_and_color_list.push_back({triangle, triangle_color});
    }

    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_);
    }

}


void Application::testDrawMeshMatrixLightNormalsCheckIntegrity()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    if (geometry_mesh.vertices.size() != geometry_mesh.normals.size())
    {
        std::cout << "mismatch from vertices and normals count!!";
        return;
    }
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }

    std::vector<std::pair<Triangle, Color>> triangle_and_color_list;

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::viewport(WIDTH, HEIGHT) * 
                                                tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    // temp light direction
    tinymath::Vec3f light_direction = {0.0f, 0.0f, 1.0f};

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        auto face_indices = geometry_mesh.faceIndices[index];
        // auto nomal_indices = geometry_mesh.faceNormalIndices[index];
        tinymath::Vec3f a = geometry_mesh.vertices[face_indices[0]];
        tinymath::Vec3f b = geometry_mesh.vertices[face_indices[1]];
        tinymath::Vec3f c = geometry_mesh.vertices[face_indices[2]];
        
        // calculate triangle normal
        tinymath::Vec3f triangle_normal = normalize(cross(b - a, c - a));
        
        float intensity = std::max(0.0f, dot(triangle_normal, light_direction));
        
        Color triangle_color{
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            static_cast<uint8_t>(intensity * 255),
            255 
        };

        tinymath::Vec4f a_transform = transformation_matrix * tinymath::toVec4(a);
        tinymath::Vec4f b_transform = transformation_matrix * tinymath::toVec4(b); 
        tinymath::Vec4f c_transform = transformation_matrix * tinymath::toVec4(c); 

        tinymath::Vec3f a_projected = tinymath::toVec3(a_transform);
        tinymath::Vec3f b_projected = tinymath::toVec3(b_transform);
        tinymath::Vec3f c_projected = tinymath::toVec3(c_transform);

        Triangle triangle{
            {{a_projected.x, a_projected.y}, a_projected.z},
            {{b_projected.x, b_projected.y}, b_projected.z},
            {{c_projected.x, c_projected.y}, c_projected.z},
        };
        
        triangle_and_color_list.push_back({triangle, triangle_color});
    }

    for (auto& [triangle, color] : triangle_and_color_list)
    {
        TriangleRasterizer::drawTriangleSolidColor(triangle, color, framebuffer_);
    }

}


void Application::testDrawMeshRandomShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    if (geometry_mesh.vertices.size() != geometry_mesh.normals.size())
    {
        std::cout << "mismatch from vertices and normals count!!";
        return;
    }
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    RandomShader random_shader(geometry_mesh, transformation_matrix);

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = random_shader.vertex(index, 0); 
        triangle_vertex[1] = random_shader.vertex(index, 1); 
        triangle_vertex[2] = random_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, random_shader, framebuffer_, true);
    }
}


void Application::testDrawMeshFaceShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    if (geometry_mesh.vertices.size() != geometry_mesh.normals.size())
    {
        std::cout << "mismatch from vertices and normals count!!";
        return;
    }
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    FaceShader face_shader(
        geometry_mesh, 
        transformation_matrix, 
        {0.0f, 0.0f, 1.0f}, // light direction 
        {255, 255, 255, 255} // base color
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = face_shader.vertex(index, 0); 
        triangle_vertex[1] = face_shader.vertex(index, 1); 
        triangle_vertex[2] = face_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, face_shader, framebuffer_, true);
    }

}


void Application::testDrawMeshGouraudShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    GouraudShader gouraud_shader(
        geometry_mesh, 
        transformation_matrix, 
        {0.0f, 0.0f, 1.0f}, // light direction 
        {255, 255, 255, 255} // base color
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = gouraud_shader.vertex(index, 0); 
        triangle_vertex[1] = gouraud_shader.vertex(index, 1); 
        triangle_vertex[2] = gouraud_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, gouraud_shader, framebuffer_, true);
    }

}


void Application::testDrawMeshLambertShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    LambertShader lambert_shader(
        geometry_mesh, 
        transformation_matrix, 
        {0.0f, 0.0f, 1.0f}, // light direction 
        {255, 255, 255, 255} // base color
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = lambert_shader.vertex(index, 0); 
        triangle_vertex[1] = lambert_shader.vertex(index, 1); 
        triangle_vertex[2] = lambert_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, lambert_shader, framebuffer_, true);
    }

}


void Application::testDrawMeshPhongShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    PhongShader phong_shader(
        geometry_mesh, 
        transformation_matrix, 
        {0.0f, 0.0f, 1.0f}, // light direction 
        tinymath::normalize(tinymath::Vec3f{2.5f, 1.0f, 2.5f}), // view direction
        {125, 125, 125, 255}, // base color
        {255, 255, 255, 255}, // specular color,
        0.05f, // ambient intensity
        200.0f // shininess 
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = phong_shader.vertex(index, 0); 
        triangle_vertex[1] = phong_shader.vertex(index, 1); 
        triangle_vertex[2] = phong_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, phong_shader, framebuffer_, true);
    }

}



void Application::testDrawMeshBlinnPhongShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    BlinnPhongShader blinn_phong_shader(
        geometry_mesh, 
        transformation_matrix, 
        {0.0f, 0.0f, 1.0f}, // light direction 
        tinymath::normalize(tinymath::Vec3f{2.5f, 1.0f, 2.5f}), // view direction
        {125, 125, 125, 255}, // base color
        {255, 255, 255, 255}, // specular color,
        0.05f, // ambient intensity
        800.0f // shininess 
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = blinn_phong_shader.vertex(index, 0); 
        triangle_vertex[1] = blinn_phong_shader.vertex(index, 1); 
        triangle_vertex[2] = blinn_phong_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, blinn_phong_shader, framebuffer_, true);
    }

}


void Application::testDrawMeshUvColorShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    UvColorShader uv_color_shader(
        geometry_mesh, 
        transformation_matrix
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = uv_color_shader.vertex(index, 0); 
        triangle_vertex[1] = uv_color_shader.vertex(index, 1); 
        triangle_vertex[2] = uv_color_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, uv_color_shader, framebuffer_, true);
    }

}



void Application::testDrawMeshTextureShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    const Texture texture = io::loadTexture("models/diablo3_pose_diffuse.tga");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    TextureShader texture_shader(
        geometry_mesh,
        texture, 
        transformation_matrix
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = texture_shader.vertex(index, 0); 
        triangle_vertex[1] = texture_shader.vertex(index, 1); 
        triangle_vertex[2] = texture_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, texture_shader, framebuffer_, true);
    }

}


void Application::testDrawMeshMaterialShader()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");
    const Texture diffuse_texture = io::loadTexture("models/diablo3_pose_diffuse.tga");
    const Texture specular_texture = io::loadTexture("models/diablo3_pose_spec.tga");
    const Texture emission_texture = io::loadTexture("models/diablo3_pose_glow.tga");
    const Texture normal_map_texture = io::loadTexture("models/diablo3_pose_nm.tga");
    
    if (geometry_mesh.faceIndices.size() != geometry_mesh.faceNormalIndices.size())
    {
        std::cout << "mismatch from faceIndex and face NormalsIndices sizes!!!";
        return;
    }  

    // transformation matrix
    tinymath::Matrix4x4 transformation_matrix = tinymath::perspective(3.0f) *
                                                tinymath::lookAt(
                                                    {2.5f, 1.0f, 2.5f}, 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
                                                );
    
    MaterialShader material_shader(
        geometry_mesh,
        diffuse_texture,
        specular_texture,
        emission_texture, 
        normal_map_texture,
        transformation_matrix,
        {0.0f, 0.0f, 1.0f}, // light direction 
        tinymath::normalize(tinymath::Vec3f{2.5f, 1.0f, 2.5f}), // view direction
        0.8f, // diffuse multiplier
        1.0f, // specular multiplier
        100.0f, // shininess
        1.0f, // emission multiplier
        0.1f // ambient multiplier
    );

    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = material_shader.vertex(index, 0); 
        triangle_vertex[1] = material_shader.vertex(index, 1); 
        triangle_vertex[2] = material_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, material_shader, framebuffer_, true);
    }
}


void Application::testDrawMeshShadowMap()
{
    framebuffer_.clear(Color{0,0,0,0});
    const Mesh geometry_mesh = io::loadObj("models/diablo3_pose.obj");

    // Light matrix
    tinymath::Matrix4x4 scale;
    scale.data[0][0] = 0.8f; // constant scale for the light in the current scene conditions.
    scale.data[1][1] = 0.8f;
    scale.data[2][2] = 0.8f;
    tinymath::Matrix4x4 light_matrix = scale * tinymath::lookAt(
                                                    tinymath::normalize(tinymath::Vec3f(1.0f, 1.0f, 1.0f)), 
                                                    {0.0f, 0.0f, 0.0f}, 
                                                    {0.0f, 1.0f, 0.0f}
    );

    // shadow map frame buffer
    Framebuffer shadow_map_buffer(WIDTH, HEIGHT);
    shadow_map_buffer.clear(Color{0,0,0,0});

    DepthShader depth_shader(geometry_mesh, light_matrix);
    for (int index = 0; index < static_cast<int>(geometry_mesh.faceIndices.size()); index++)
    {
        std::array<tinymath::Vec4f,3> triangle_vertex; 
        triangle_vertex[0] = depth_shader.vertex(index, 0); 
        triangle_vertex[1] = depth_shader.vertex(index, 1); 
        triangle_vertex[2] = depth_shader.vertex(index, 2);
        
        TriangleRasterizer::drawTriangle(triangle_vertex, depth_shader, shadow_map_buffer, true);
    }

    blitDepthAsGrayscale(shadow_map_buffer);
}
