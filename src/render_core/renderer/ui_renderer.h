#ifndef RENDER_CORE_RENDERER_UI_RENDERER_H_
#define RENDER_CORE_RENDERER_UI_RENDERER_H_
#include <volk.h>

namespace rdc{
class UiRenderer {
  int _ui_width = 800;
  int _ui_height = 600;
  VkImageView _render_target_view = VK_NULL_HANDLE;

  static void InitImGuiRender();

 public:
  void SetUiSize(int width, int height) {
    _ui_width = width;
    _ui_height = height;
  }
  void SetRenderTargetView(const VkImageView &view) {
    _render_target_view = view;
  }

  void RecordUiCommandBuffer(VkCommandBuffer command_buffer);

  UiRenderer();
  ~UiRenderer();
};

}  // namespace rdc
#endif  // SRC_RENDER_CORE_RENDERER_UI_RENDERER_H_
