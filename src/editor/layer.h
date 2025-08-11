#ifndef EDITOR_LAYER_H_
#define EDITOR_LAYER_H_
#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "editor/types.hpp"
#include "tools.hpp"

namespace editor {

class Layer {
  std::vector<Layer*> _child;
  std::unique_ptr<void*, void (*)(void*)> _meta_data;
  std::string _layer_name;

 public:
  enum LayerDataType : uint8_t {
    kImageLayer,
    kDirLayer,
    kMorpherLayer,
    kUnknown,
  };

  Layer();
  LayerDataType GetType() const { return _type; }
  void SetType(LayerDataType type) { _type = type; }

  bool HasChild() const;
  std::span<Layer* const> GetChild() const;
  std::span<Layer*> GetChild();
  void AddChild(Layer* child) {
    assert(HasChild() && "Layer does not support child");
    _child.push_back(child);
  }
  template <typename T>
  T* GetLayerData() const {
    assert(_type != kUnknown && "Layer type is unknown");
    assert(_type == T::Type && "Layer type mismatch");
    return static_cast<T*>(_meta_data.get());
  }
  void SetLayerData(std::unique_ptr<void*, void (*)(void*)> data);

  std::string GetLayerName() const { return _layer_name; }
  void SetLayerName(const std::string& name) { _layer_name = name; }
  ~Layer();

 private:
  LayerDataType _type = kUnknown;
};

struct ImageLayerData {
  static constexpr Layer::LayerDataType Type = Layer::kImageLayer;
  CPUImage* image = nullptr;
  Property<bool> is_visible{true};
  ~ImageLayerData() = default;
};

}  // namespace editor

#endif  // EDITOR_LAYER_H_
