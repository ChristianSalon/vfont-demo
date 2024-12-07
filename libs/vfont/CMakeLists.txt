cmake_minimum_required(VERSION 3.25)
project(vfont VERSION 1.0)

set(LIB_NAME vfont)
set(VFONT_HEADERS
    include/VFONT/renderer.h
    include/VFONT/renderer_decorator.h
    include/VFONT/text_renderer.h
    include/VFONT/timed_renderer.h
    include/VFONT/text_renderer_utils.h
    include/VFONT/glyph.h
    include/VFONT/glyph_mesh.h
    include/VFONT/glyph_compositor.h
    include/VFONT/glyph_cache.h
    include/VFONT/character.h
    include/VFONT/font.h
    include/VFONT/text_block.h
    include/VFONT/tessellator.h
    include/VFONT/cpu_tessellator.h
    include/VFONT/gpu_tessellator.h
    include/VFONT/combined_tessellator.h
    include/VFONT/drawer.h
    include/VFONT/cpu_drawer.h
    include/VFONT/gpu_drawer.h
    include/VFONT/combined_drawer.h
)
set(VFONT_SOURCES
    src/renderer_decorator.cpp
    src/text_renderer.cpp
    src/timed_renderer.cpp
    src/glyph.cpp
    src/glyph_mesh.cpp
    src/glyph_compositor.cpp
    src/glyph_cache.cpp
    src/character.cpp
    src/font.cpp
    src/text_block.cpp
    src/tessellator.cpp
    src/cpu_tessellator.cpp
    src/gpu_tessellator.cpp
    src/combined_tessellator.cpp
    src/drawer.cpp
    src/cpu_drawer.cpp
    src/gpu_drawer.cpp
    src/combined_drawer.cpp
)

add_library(${LIB_NAME} STATIC ${VFONT_HEADERS} ${VFONT_SOURCES})

target_include_directories(${LIB_NAME} PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/VFONT>
    $<INSTALL_INTERFACE:include/VFONT>
)
target_include_directories(${LIB_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# link Vulkan
target_include_directories(${LIB_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${LIB_NAME} PRIVATE Vulkan::Vulkan)

# link freetype
target_link_libraries(${LIB_NAME} PUBLIC freetype)

# link CDT
target_link_libraries(${LIB_NAME} PRIVATE CDT::CDT)

# link glm
target_link_libraries(${LIB_NAME} PUBLIC glm::glm)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)

# compile vertex and fragment shaders with glslc
message(STATUS "vfont - compiling shaders")
if(NOT Vulkan_glslc_FOUND)
    message(FATAL_ERROR "Error compiling shaders, glslc not found")
endif()

add_custom_target(vfont-shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle-vert.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle-frag.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/curve-vert.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.tesc" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/curve-tesc.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.tese" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/curve-tese.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/curve-frag.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/winding_number.vert" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/winding_number-vert.spv"
    COMMAND ${Vulkan_GLSLC_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/shaders/winding_number.frag" -o "${CMAKE_CURRENT_BINARY_DIR}/shaders/winding_number-frag.spv"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.tesc"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.tese"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/curve.frag"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/winding_number.vert"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/winding_number.frag"
)

add_dependencies(${LIB_NAME} vfont-shaders)
