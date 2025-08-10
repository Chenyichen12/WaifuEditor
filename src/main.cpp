#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

#include "editor/document.h"
#include "editor/gui.h"
#include "render_core/renderer/renderer.h"
#include "render_core/vulkan_driver.h"

class AppWindow {
  rdc::ApplicationRenderer *_renderer = nullptr;
  rdc::RenderResourceManager *_resource_manager = nullptr;
  editor::Gui *_gui = nullptr;

 public:
  AppWindow() {
    if (!glfwInit()) {
      std::abort();
    }
    _gui = new editor::Gui();
    // _gui->WindowResizeSignal.connect([this](int width, int height) {
    // });

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

    // _driver = new rdc::VulkanDriver(config);
    rdc::VulkanDriver::InitSingleton(config);
    _renderer = new rdc::ApplicationRenderer();
    auto *model_renderer = _renderer->GetModelRenderer();

    // read layer
    _resource_manager = new rdc::RenderResourceManager();
    {
      nlohmann::json layer_config;
      std::ifstream layer_file("../test/layers.json");
      if (!layer_file.is_open()) {
        std::cerr << "Failed to open layers.json\n";
        std::abort();
      }
      layer_file >> layer_config;
      layer_file.close();

      for (const auto &layer : layer_config["layers"]) {
        auto vertex_struct = layer["vertices"];
        auto position_array =
            vertex_struct["position"].get<std::vector<float>>();
        auto uv_array = vertex_struct["uv"].get<std::vector<float>>();
        auto index_array = vertex_struct["index"].get<std::vector<uint32_t>>();

        std::string file_path = layer["path"];
        file_path = "../test/" + file_path;
        CPUImage cpu_image;
        cpu_image.LoadFromFile(file_path);
        if (!cpu_image.IsValid()) {
          std::cerr << "Failed to load image: " << file_path << "\n";
          continue;
        }
        rdc::Layer2dResource::ImageConfig image_config;
        image_config.pimage = &cpu_image;
        image_config.format = VK_FORMAT_R8G8B8A8_UNORM;
        std::vector<rdc::ModelVertex> vertices(position_array.size() / 2);
        for (int i = 0; i < position_array.size(); i += 2) {
          auto &vertex = vertices[i / 2];
          vertex.position.x = position_array[i];
          vertex.position.y = position_array[i + 1];
          vertex.uv.x = uv_array[i];
          vertex.uv.y = uv_array[i + 1];
        }

        rdc::Layer2dResource::ImageConfig const texture_config{
            .pimage = &cpu_image,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .vertices = vertices,
            .indices = index_array,
        };
        auto layer_resource =
            rdc::Layer2dResource::CreateFromImage(texture_config);
        if (!layer_resource) {
          std::cerr << "Failed to create layer resource from image\n";
          continue;
        }
        auto *res = _resource_manager->AddResource(std::move(layer_resource));
        model_renderer->AddLayer(res);
      }
      const uint32_t canvas_width =
          layer_config["canvas"]["width"].get<uint32_t>();
      const uint32_t canvas_height =
          layer_config["canvas"]["height"].get<uint32_t>();
      model_renderer->SetCanvasSize(canvas_width, canvas_height);
    }
  }
  void Run() {
    while (!glfwWindowShouldClose(_gui->GetWindow())) {
      glfwPollEvents();
      _gui->TickGui();
      _renderer->Render();
    }
  }
  ~AppWindow() {
    delete _renderer;
    delete _resource_manager;
    delete _gui;

    editor::EditorConfig *config = editor::EditorConfig::GetInstance();
    config->SaveConfig();
    rdc::VulkanDriver::CleanupSingleton();
    glfwTerminate();
  }
};

int main() {
  AppWindow window;
  window.Run();
  return 0;
}