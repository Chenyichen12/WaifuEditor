#ifndef RENDER_CORE_RENDERER_MODEL_RENDERER_H_
#define RENDER_CORE_RENDERER_MODEL_RENDERER_H_

#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <span>
#include "render_core/rdres.hpp"
#include "editor/types.hpp"

namespace rdc {
    struct ModelVertex {
  glm::vec2 position;
  glm::vec2 uv;
};

class Layer2dResource : public IRenderResource, public NoCopyable {
  // friend class ModelRenderer;

  VkImage _image = VK_NULL_HANDLE;
  VmaAllocation _allocation = VK_NULL_HANDLE;
  VkImageView _image_view = VK_NULL_HANDLE;
  Layer2dResource() = default;
  struct Buffer {
    VkBuffer _buffer = VK_NULL_HANDLE;
    VmaAllocation _allocation = VK_NULL_HANDLE;
  };
  Buffer _vertex_buffer;
  Buffer _index_buffer;
  std::vector<ModelVertex> _vertices;
  std::vector<uint32_t> _indices;
  int _dirty_flag = 0;  // 1: dirt, 2: need recreate, 0: clean

 public:
  struct ImageConfig {
    CPUImage *pimage = nullptr;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    std::span<ModelVertex> vertices;
    std::span<uint32_t> indices;
  };
  static std::unique_ptr<Layer2dResource> CreateFromImage(
      const ImageConfig &config);
  VkBuffer GetVertexBuffer() const { return _vertex_buffer._buffer; }
  VkBuffer GetIndexBuffer() const { return _index_buffer._buffer; }
  VkImage GetImage() const { return _image; }
  VkImageView GetImageView() const { return _image_view; }
  uint32_t GetIndexCount() const { return _indices.size(); }
  void SetVertex(std::span<ModelVertex> vertices, std::span<uint32_t> indices);
  bool IsBufferDirty() const { return _dirty_flag != 0; }
  void RefreshBuffer();

  ~Layer2dResource() override;
};

class ModelRenderer {
  // render resources use to render layer
  std::vector<Layer2dResource *> _render_layers;
  VkDescriptorSetLayout _descriptor_set_layout = VK_NULL_HANDLE;
  VkPipelineLayout _pipeline_layout = VK_NULL_HANDLE;
  VkSampler _sampler = VK_NULL_HANDLE;

  struct Shader {
    VkShaderStageFlagBits stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
    VkShaderEXT shader = VK_NULL_HANDLE;
    void Destroy(const VkDevice &device) {
      vkDestroyShaderEXT(device, shader, nullptr);
      shader = VK_NULL_HANDLE;
    };
  };
  Shader _vertex_shader;
  Shader _fragment_shader;
  struct {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void Destroy(const VmaAllocator &allocator) {
      vmaDestroyBuffer(allocator, buffer, allocation);
    }
  } _ubo_buffer;

  VkImageView _render_target_view = VK_NULL_HANDLE;

  struct Region {
    int x;
    int y;
    uint32_t width;
    uint32_t height;
  } _region = {.x = 0, .y = 0, .width = 800, .height = 600};

  float _canvas_scale = 1.0f;
  glm::vec2 _canvas_offset = glm::vec2(0.0f);

  uint32_t _canvas_width = 800;
  uint32_t _canvas_height = 600;

  void UpdateUniform();
  // cmd
  void BindLayerDrawCommand(VkCommandBuffer command_buffer,
                            uint32_t index) const;

 public:
  ModelRenderer();
  void AddLayer(Layer2dResource *layer);
  std::span<Layer2dResource *> GetLayers() { return _render_layers; }
  ~ModelRenderer();

  void SetRegion(int pos_x, int pos_y, uint32_t width, uint32_t height);
  void SetCanvasSize(uint32_t width, uint32_t height);
  void AutoCenterCanvas();
  void SetCanvasOffset(float x, float y) {
    _canvas_offset.x = x;
    _canvas_offset.y = y;
  }

  void SetTargetView(VkImageView view) { _render_target_view = view; }
  void PrepareRender();
  void RecordCommandBuffer(VkCommandBuffer command_buffer);
};
}

#endif  // RENDER_CORE_RENDERER_MODEL_RENDERER_H_
