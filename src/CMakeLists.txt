

set(waifu_source main.cpp)


# sources
file(GLOB waifu_render_core_source ./render_core/*.cpp ./render_core/*.h)
file(GLOB waifu_editor_source ./editor/*.cpp, ./editor/*.h)


#
find_package(Vulkan REQUIRED)

add_executable(
    waifu_studio 
    ${waifu_source}
    ${waifu_render_core_source}
    ${waifu_editor_source}
)

target_link_libraries(waifu_studio PUBLIC glfw imgui single_head)
target_compile_definitions(waifu_studio PUBLIC VK_NO_PROTOTYPES)
target_include_directories(waifu_studio PUBLIC ${Vulkan_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})