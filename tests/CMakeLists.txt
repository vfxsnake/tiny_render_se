# Framebuffer Tests block
add_executable(test_framebuffer
    test_framebuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/rasterizer/Framebuffer.cpp
)

target_include_directories(test_framebuffer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_framebuffer PRIVATE
    Catch2::Catch2WithMain
)

# Vect2 test block
add_executable(test_vec2
    test_vec2.cpp
    ${CMAKE_SOURCE_DIR}/src/math/Vec2.h
)

target_include_directories(test_vec2 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_vec2 PRIVATE
    Catch2::Catch2WithMain
)

# LineDrawer test block
add_executable(test_linedrawer
    test_linedrawer.cpp
    ${CMAKE_SOURCE_DIR}/src/rasterizer/LineDrawer.cpp
    ${CMAKE_SOURCE_DIR}/src/rasterizer/Framebuffer.cpp
)

target_include_directories(test_linedrawer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_linedrawer PRIVATE
    Catch2::Catch2WithMain
)

# TriangleRasterizer test block
add_executable(test_triangle_rasterizer
    test_triangle_rasterizer.cpp
    ${CMAKE_SOURCE_DIR}/src/rasterizer/TriangleRasterizer.cpp
    ${CMAKE_SOURCE_DIR}/src/rasterizer/Framebuffer.cpp
)

target_include_directories(test_triangle_rasterizer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_triangle_rasterizer PRIVATE
    Catch2::Catch2WithMain
)

# appending test
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_framebuffer)
catch_discover_tests(test_vec2)
catch_discover_tests(test_linedrawer)
catch_discover_tests(test_triangle_rasterizer)
