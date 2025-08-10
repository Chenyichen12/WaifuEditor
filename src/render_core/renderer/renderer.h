#ifndef RENDER_CORE_RENDERER_H_
#define RENDER_CORE_RENDERER_H_

#include "model_renderer.h"
#include "ui_renderer.h"
namespace rdc {


class ApplicationRenderer {
  int _window_width = 800;
  int _window_height = 600;
  std::unique_ptr<ModelRenderer> _model_renderer;
  std::unique_ptr<UiRenderer> _ui_renderer;

  VkCommandBuffer _app_command_buffer = VK_NULL_HANDLE;
  VkSemaphore _swapchain_image_available_semaphore = VK_NULL_HANDLE;
  VkSemaphore _graphics_finished_semaphore = VK_NULL_HANDLE;

 public:
  ApplicationRenderer();
  ~ApplicationRenderer();
  void SetWindowSize(int width, int height);
  void Render();
  ModelRenderer *GetModelRenderer() const { return _model_renderer.get(); }
  UiRenderer *GetUiRenderer() const { return _ui_renderer.get(); }
};

}  // namespace rdc
#endif  // RENDER_CORE_RENDERER_H_
