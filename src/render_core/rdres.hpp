#ifndef RENDER_CORE_RDRES_HPP_
#define RENDER_CORE_RDRES_HPP_

#include "vulkan_driver.h"
#include "tools.hpp"
namespace rdc {
class IRenderResource {
  friend class RenderResourceManager;

 protected:
  uint32_t _id = UINT32_MAX;

 public:
  uint32_t GetId() const { return _id; }
  virtual ~IRenderResource() = default;
};

class ImageRenderResource : public IRenderResource {
  VkImage _image = VK_NULL_HANDLE;
  VmaAllocation _allocation = VK_NULL_HANDLE;
  VkImageView _image_view = VK_NULL_HANDLE;
  VulkanDriver *_driver = nullptr;

 public:
  ImageRenderResource(VkImage image, VmaAllocation allocation,
                      VkImageView image_view, VulkanDriver *driver)
      : _image(image),
        _allocation(allocation),
        _image_view(image_view),
        _driver(driver) {}
  ~ImageRenderResource() {
    vkDestroyImageView(_driver->GetDevice(), _image_view, nullptr);
    vmaDestroyImage(_driver->GetVmaAllocator(), _image, _allocation);
  }
};

class RenderResourceManager {
  std::unordered_map<uint32_t, std::unique_ptr<IRenderResource>> _resources;
  IdAllocator _id_allocator;

 public:
  template <typename T, typename... Args>
  T *CreateResource(Args &&...args) {
    auto resource = std::make_unique<T>(std::forward<Args>(args)...);
    resource->_id = _id_allocator.AllocateId();
    T *ptr = resource.get();
    _resources[ptr->_id] = std::move(resource);
    return ptr;
  }
  template <typename T>
  T *AddResource(std::unique_ptr<T> resource) {
    resource->_id = _id_allocator.AllocateId();
    T *ptr = resource.get();
    _resources[ptr->_id] = std::move(resource);
    return ptr;
  }
  template <typename T>
  T *AddResource(T *resource) {
    resource->_id = _id_allocator.AllocateId();
    T *ptr = resource;
    _resources[ptr->_id] = std::unique_ptr<T>(resource);
    return ptr;
  }

  template <typename T>
  T *GetResource(uint32_t id) {
    return static_cast<T *>(_resources[id].get());
  }

  void ReleaseResource(uint32_t id) { _resources.erase(id); }
};
}  // namespace rdc
#endif  // RENDER_CORE_RDRES_HPP_
