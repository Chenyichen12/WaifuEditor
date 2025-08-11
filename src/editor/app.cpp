#include "app.h"

#include <cstdlib>
#include <memory>

#include "GLFW/glfw3.h"
#include "document.h"
#include "render_core/renderer/renderer.h"
#include "render_core/vulkan_driver.h"
#include "tools.hpp"

namespace editor {
void App::AppInitContext() {
  if (!glfwInit()) {
    std::abort();
  }
  _gui = std::make_unique<Gui>();

  rdc::VulkanDriverConfig config;
  config.initial_height = 600;
  config.initial_width = 800;
  config.create_surface_callback = [this](VkInstance instance,
                                          VkSurfaceKHR &surface) {
    return this->_gui->CreateVulkanSurface(instance, surface);
  };

  {
    uint32_t glfw_extensions = 0;
    auto *extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions);
    config.instance_extensions =
        std::vector<const char *>(extensions, extensions + glfw_extensions);
  }
  {
    config.instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
    config.instance_layers.emplace_back("VK_LAYER_KHRONOS_shader_object");
  }

  rdc::VulkanDriver::InitSingleton(config);
  _renderer = std::make_unique<rdc::ApplicationRenderer>();
  auto *model_renderer = _renderer->GetModelRenderer();
}

App::App(int argc, char **argv) {
  WaifuUnused(argc, argv);
  AppInitContext();
}
void App::OpenDocument(std::unique_ptr<Document> doc){
  _current_document = std::move(doc);
}
void App::Exec() {
  while (!glfwWindowShouldClose(_gui->GetWindow())) {
    glfwPollEvents();
    _gui->TickGui();
    _renderer->Render();
  }
}
App::~App() {
  _renderer.reset();
  _gui.reset();

  EditorConfig *config = EditorConfig::GetInstance();
  config->SaveConfig();
  rdc::VulkanDriver::CleanupSingleton();
  glfwTerminate();
}
}  // namespace editor