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
    src/rasterizer/LineDrawer.cpp
    src/rasterizer/TriangleRasterizer.cpp
    src/display/DisplayPipeline.cpp
    src/display/GraphicsPipeline.cpp
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

# Shaders block
if(WIN32)
    set(SLANG_C $ENV{VULKAN_SDK}/Bin/slangc.exe)
else()
    set(SLANG_C $ENV{VULKAN_SDK}/bin/slangc)
endif()

set(SHADERS_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_SOURCES ${SHADERS_DIR}/display.slang)
set(DISPLAY_ENTRY_POINTS -entry vertMain -entry fragMain)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/shaders/display.spv 
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${SLANG_C} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${DISPLAY_ENTRY_POINTS} -o ${CMAKE_BINARY_DIR}/shaders/display.spv 
    WORKING_DIRECTORY ${SHADERS_DIR}
    DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
    COMMENT "compiling slang shaders"
    VERBATIM
)

add_custom_target(Shaders DEPENDS 
    ${CMAKE_BINARY_DIR}/shaders/display.spv
)
add_dependencies(TinyRendererSE Shaders)

# ── Tests ─────────────────────────────────────────────────────────────────────
add_subdirectory(tests)
