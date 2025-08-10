#ifndef RENDER_CORE_RDRES_HPP_
#define RENDER_CORE_RDRES_HPP_

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
  T *GetResource(uint32_t rid) {
    return static_cast<T *>(_resources[rid].get());
  }

  void ReleaseResource(uint32_t rid) { _resources.erase(rid); }
};
}  // namespace rdc
#endif  // RENDER_CORE_RDRES_HPP_
