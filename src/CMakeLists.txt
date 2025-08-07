

set(waifu_source main.cpp)


# sources
file(GLOB waifu_render_core_source ./render_core/*.cpp ./render_core/*.h)
file(GLOB waifu_editor_source ./editor/*.cpp, ./editor/*.h)


#
find_package(Vulkan REQUIRED)

add_executable(
    waifu_editor 
    ${waifu_source}
    ${waifu_render_core_source}
    ${waifu_editor_source}
)

target_link_libraries(waifu_editor PUBLIC glfw imgui single_head)
target_compile_definitions(waifu_editor PUBLIC VK_NO_PROTOTYPES GLM_FORCE_STD140)
target_include_directories(waifu_editor PUBLIC ${Vulkan_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})