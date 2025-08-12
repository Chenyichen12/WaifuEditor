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
  _gui->DocumentOpenSignal.connect([this](const std::string &path) {
    auto doc = Document::LoadFromLayerConfig(path);
    this->OpenDocument(std::move(doc));
  });

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
}

App::App(int argc, char **argv) {
  WaifuUnused(argc, argv);
  AppInitContext();
}
void App::OpenDocument(std::unique_ptr<Document> doc) {
  _current_document = std::move(doc);

  // layers
  auto *root_layer = _current_document->GetRootLayer();
  auto it = LayerIterator(root_layer);
  auto end_it = it.end();
  for (; it != end_it; ++it) {
    auto *layer = (*it).layer;
    if (layer->GetType() == Layer::kImageLayer) {
      // handle image
      auto *image_data = layer->GetLayerData<ImageLayerData>();

      // layer resource
      rdc::Layer2dResource::ImageConfig image_config;
      image_config.pimage = image_data->image;
      std::vector<rdc::ModelVertex> vertices;

      {
        for (size_t i = 0; i < image_data->points.size(); ++i) {
          rdc::ModelVertex vertex;
          vertex.position = image_data->points[i];
          vertex.uv = image_data->uvs[i];
          vertices.push_back(vertex);
        }
      }

      image_config.vertices = vertices;
      image_config.indices = image_data->indices;
      auto layer_resource = rdc::Layer2dResource::CreateFromImage(image_config);
      _renderer->GetModelRenderer()->AddLayer(layer_resource.get());
      _renderer->GetResourceManager()->AddResource(std::move(layer_resource));
    }
  }
  auto model_renderer = _renderer->GetModelRenderer();
  model_renderer->SetCanvasSize(_current_document->GetCanvasSize().x, _current_document->GetCanvasSize().y);
  model_renderer->AutoCenterCanvas();
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