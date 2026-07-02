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

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(test_framebuffer)
