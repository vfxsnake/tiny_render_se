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

# appending test
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_framebuffer)
catch_discover_tests(test_vec2)
