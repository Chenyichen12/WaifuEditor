#ifndef EDITOR_LAYER_H_
#define EDITOR_LAYER_H_
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <glm/vec2.hpp>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "editor/types.hpp"
#include "tools.hpp"

namespace editor {

enum LayerDataType : uint8_t {
  kImageLayer,
  kDirLayer,
  kMorpherLayer,
  kUnknown,
};

class LayerData {
 public:
  virtual LayerDataType Type() const = 0;
  virtual ~LayerData() = default;
  virtual void Serialize(nlohmann::json& json) const = 0;
  virtual void Deserialize(const nlohmann::json& json) = 0;

  static std::unique_ptr<LayerData> Create(LayerDataType type, const nlohmann::json& json);
};

class Layer {
  std::vector<Layer*> _child;
  std::unique_ptr<LayerData> _meta_data = nullptr;
  std::string _layer_name;

 public:
  Layer();
  Layer(const std::string& layer_name, std::unique_ptr<LayerData> meta_data);
  LayerDataType GetType() const { return _meta_data->Type(); }

  bool HasChild() const;
  std::span<Layer* const> GetChild() const;
  std::span<Layer*> GetChild();
  void AddChild(Layer* child) {
    assert(HasChild() && "Layer does not support child");
    _child.push_back(child);
  }
  LayerData* GetLayerData() const { return _meta_data.get(); }
  template <typename T>
  T* GetLayerData() const { return static_cast<T*>(_meta_data.get()); }
  void SetLayerData(std::unique_ptr<LayerData> meta_data);
  std::string GetLayerName() const { return _layer_name; }
  void SetLayerName(const std::string& name) { _layer_name = name; }

  // back translate children
  ~Layer();

  class LayerIterator {
    Layer* _root = nullptr;

   public:
    explicit LayerIterator(Layer* root, bool front_iter = true);
    struct Element {
      Layer* layer;
      size_t depth;
    };
    const Element& operator*() const { return get(); };
    const Element& get() const;
    LayerIterator& operator++();

    bool operator!=(const LayerIterator& other) const;

   private:
    std::vector<Element> _stack;
    bool _front_iter = true;  // true for front, false for back
  };
  LayerIterator BeginBackIter() { return LayerIterator(this, false); }
  LayerIterator EndBackIter() { return LayerIterator(nullptr); }

  LayerIterator BeginFrontIter() { return LayerIterator(this); }
  LayerIterator EndFrontIter() { return LayerIterator(nullptr); }

 private:
};


struct DirLayerData : public LayerData{
  DirLayerData() = default;
  LayerDataType Type() const override { return kDirLayer; }
  void Serialize(nlohmann::json& json) const override{}
  void Deserialize(const nlohmann::json& json) override{}
};

struct ImageLayerData : public LayerData {
  // the relative path to the project file
  int image_id = -1;
  CPUImage* image = nullptr;
  Property<bool> is_visible{true};

  std::vector<glm::vec2> points;
  std::vector<glm::vec2> uvs;
  std::vector<uint32_t> indices;

  LayerDataType Type() const override { return kImageLayer; }
  void Serialize(nlohmann::json& json) const override;
  void Deserialize(const nlohmann::json& json) override;
  ~ImageLayerData() override = default;
};

}  // namespace editor

#endif  // EDITOR_LAYER_H_
