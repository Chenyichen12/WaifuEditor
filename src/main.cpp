
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

#include "render_core/renderer.h"
#include "render_core/vulkan_driver.h"
class AppWindow {
  GLFWwindow *window;
  rdc::ModelRenderer* renderer = nullptr;

  VkResult CreateVulkanSurface(VkInstance instance, VkSurfaceKHR &surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
        VK_SUCCESS) {
      std::cerr << "Failed to create Vulkan surface\n";
      return VK_ERROR_UNKNOWN;
    }
    return VK_SUCCESS;
  }

 public:
  AppWindow() {
    if (!glfwInit()) {
      std::abort();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow(800, 600, "WaifuStudio", nullptr, nullptr);

    if (!window) {
      std::cerr << "fail to create window\n";
      std::abort();
    }

    rdc::VulkanDriverConfig config;
    config.initial_height = 600;
    config.initial_width = 800;
    config.create_surface_callback = [this](VkInstance instance,
                                            VkSurfaceKHR &surface) {
      return CreateVulkanSurface(instance, surface);
    };
    // vulkan init
    {
      uint32_t glfwExtensions = 0;
      auto extensions = glfwGetRequiredInstanceExtensions(&glfwExtensions);
      config.instance_extensions =
          std::vector<const char *>(extensions, extensions + glfwExtensions);
    }
    {
      config.instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
      config.instance_layers.emplace_back("VK_LAYER_KHRONOS_shader_object");
    }

    // _driver = new rdc::VulkanDriver(config);
    rdc::VulkanDriver::InitSingleton(config);
    renderer = new rdc::ModelRenderer();

  }
  void Run() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }
  ~AppWindow() {
    delete renderer;
    rdc::VulkanDriver::CleanupSingleton();

    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

int main() {
  AppWindow window;
  window.Run();
  return 0;
}