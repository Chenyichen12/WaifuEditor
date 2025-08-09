#include "gui.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <iostream>

#include "render_core/vulkan_driver.h"

namespace editor {

void Gui::WindowResizeCallback(GLFWwindow *window, int width, int height) {
  auto *gui = static_cast<Gui *>(glfwGetWindowUserPointer(window));
  if (gui) {
    gui->WindowResizeSignal(width, height);
  };
  auto *driver = rdc::VulkanDriver::GetSingleton();
  if (driver) {
    driver->MarkSwapchainInvalid();
  }
}

Gui::Gui() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  _window = glfwCreateWindow(800, 600, "WaifuStudio", nullptr, nullptr);

  if (!_window) {
    std::cerr << "fail to create window\n";
    std::abort();
  }

  // set resize callback
  glfwSetWindowUserPointer(_window, this);
  glfwSetWindowSizeCallback(_window, WindowResizeCallback);
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &imgui_io = ImGui::GetIO();
    (void)imgui_io;
    imgui_io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(_window, true);
  }
}

Gui::~Gui() {
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  if (_window) {
    glfwDestroyWindow(_window);
    _window = nullptr;
  }
}

void Gui::TickGui() {
  ImGui_ImplGlfw_NewFrame();
  ImGui_ImplVulkan_NewFrame();
  ImGui::NewFrame();

  ImGui::ShowDemoWindow();
}
void Gui::GetWindowSize(int &width, int &height) const {
  if (_window) {
    glfwGetFramebufferSize(_window, &width, &height);
  } else {
    width = 0;
    height = 0;
  }
}

VkResult Gui::CreateVulkanSurface(VkInstance instance,
                                  VkSurfaceKHR &surface) const {
  if (!_window) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  return glfwCreateWindowSurface(instance, _window, nullptr, &surface);
}

}  // namespace editor