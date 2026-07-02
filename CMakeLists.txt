cmake_minimum_required(VERSION 3.25)
project(TinyRendererSE LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

# ── Vulkan SDK ─────────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

# ── FetchContent dependencies ──────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    VulkanHpp
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(VulkanHpp)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ── Engine source reference ────────────────────────────────────────────────────
set(ENGINE_SRC ${CMAKE_SOURCE_DIR}/engine/vk_tutorial_se/src)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(TinyRendererSE
    src/main.cpp
    src/Application.cpp
    src/rasterizer/Framebuffer.cpp
    ${ENGINE_SRC}/core/VulkanContext.cpp
    ${ENGINE_SRC}/core/SwapChain.cpp
)

target_include_directories(TinyRendererSE PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${ENGINE_SRC}
    ${stb_SOURCE_DIR}
)

target_link_libraries(TinyRendererSE PRIVATE
    Vulkan::Vulkan
    glfw
    Vulkan::Headers
)

target_compile_definitions(TinyRendererSE PRIVATE
    VULKAN_HPP_NO_CONSTRUCTORS
)

# ── Tests ─────────────────────────────────────────────────────────────────────
add_subdirectory(tests)
