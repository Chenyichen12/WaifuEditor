
add_subdirectory(glfw)
# add_subdirectory(freetype)

find_package(Vulkan REQUIRED)
set(imgui_source
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_demo.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        # imgui/misc/freetype/imgui_freetype.cpp
)
add_library(imgui ${imgui_source})
target_compile_definitions(imgui PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_link_libraries(imgui PRIVATE glfw PRIVATE Vulkan::Headers)
target_include_directories(imgui PUBLIC ./imgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/volk)

# single head
add_library(single_head single_head.cpp)
target_include_directories(single_head
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glm
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/volk
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/json/single_include
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sigslot/include
)
target_link_libraries(single_head PRIVATE Vulkan::Headers)