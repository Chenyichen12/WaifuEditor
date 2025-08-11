#ifndef RENDER_CORE_RENDERER_UI_RENDERER_H_
#define RENDER_CORE_RENDERER_UI_RENDERER_H_
#include <volk.h>
#include <vk_mem_alloc.h>
#include <string>
#include <memory>
#include <unordered_map>

#include "editor/types.hpp"
#include "imgui.h"

namespace rdc{

class StaticUiResource{
  friend class UiRenderer;
  VkImage _image = VK_NULL_HANDLE;
  VkImageView _image_view = VK_NULL_HANDLE;
  VmaAllocation _allocation = VK_NULL_HANDLE;
  ImTextureID _texture_id = 0;
  std::string _res_name;

 public:
  StaticUiResource(const std::string& res_name, const CPUImage* image);
  ImTextureID GetTextureId() const { return _texture_id; }
  std::string GetResourceName() const { return _res_name; }
  ~StaticUiResource();
};

class UiRenderer {
  int _ui_width = 800;
  int _ui_height = 600;
  VkImageView _render_target_view = VK_NULL_HANDLE;

  VkSampler _ui_sampler = VK_NULL_HANDLE;

  static void InitImGuiRender();
  std::unordered_map<std::string, std::unique_ptr<StaticUiResource>>
      _static_resources;

 public:
  void SetUiSize(int width, int height) {
    _ui_width = width;
    _ui_height = height;
  }
  void SetRenderTargetView(const VkImageView &view) {
    _render_target_view = view;
  }

  void RecordUiCommandBuffer(VkCommandBuffer command_buffer);
  void AddStaticResource(const std::string& res_name, const CPUImage* image);
  ImTextureID GetStaticResourceId(const std::string& res_name);

  UiRenderer();
  ~UiRenderer();
};

}  // namespace rdc
#endif  // SRC_RENDER_CORE_RENDERER_UI_RENDERER_H_
