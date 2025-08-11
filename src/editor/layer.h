#ifndef EDITOR_LAYER_H_
#define EDITOR_LAYER_H_
#include <cassert>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include <glm/vec2.hpp>

#include "editor/types.hpp"
#include "tools.hpp"

namespace editor {

class Layer {
  std::vector<Layer*> _child;
  void* _meta_data = nullptr;
  std::function<void(void*)> _meta_data_deleter = [](void* ptr) { free(ptr); };
  std::string _layer_name;

 public:
  enum LayerDataType : uint8_t {
    kImageLayer,
    kDirLayer,
    kMorpherLayer,
    kUnknown,
  };

  Layer();
  Layer(const std::string& layer_name, LayerDataType data_type);
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
    return reinterpret_cast<T*>(_meta_data);
  }
  // void SetLayerData(std::unique_ptr<std::any> meta_data);
  void SetOwnerLayerData(void* meta_data, const std::function<void(void*)>& deleter);


  std::string GetLayerName() const { return _layer_name; }
  void SetLayerName(const std::string& name) { _layer_name = name; }


  // back translate children
  ~Layer();

 private:
  LayerDataType _type = kUnknown;
};

class LayerIterator {
  Layer* _root = nullptr;
  std::vector<Layer*> _stack;

  Layer* operator*() const {
    return _stack.back();
  }

};

struct ImageLayerData {
  static constexpr Layer::LayerDataType Type = Layer::kImageLayer;
  CPUImage* image = nullptr;
  Property<bool> is_visible{true};

  std::vector<glm::vec2> points;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;

  ~ImageLayerData() = default;
};

}  // namespace editor

#endif  // EDITOR_LAYER_H_
