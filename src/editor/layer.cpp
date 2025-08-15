#include "layer.h"

#include <any>
#include <memory>
#include <vector>

#include "tools.hpp"

namespace editor {

std::unique_ptr<LayerData> LayerData::Create(LayerDataType type,
                                             const nlohmann::json& json) {
  std::unique_ptr<LayerData> data;
  switch (type) {
    case kImageLayer:
      data = std::make_unique<ImageLayerData>();
      data->Deserialize(json);
      break;
    case kDirLayer:
      data = std::make_unique<DirLayerData>();
      data->Deserialize(json);
      break;
    case kMorpherLayer:
      break;
    default:
      assert(false && "Unsupported layer type");
      break;
  }
  return data;
}
Layer::Layer() = default;
Layer::Layer(const std::string& layer_name,
             std::unique_ptr<LayerData> meta_data) {
  _layer_name = layer_name;
  _meta_data = std::move(meta_data);
}

bool Layer::HasChild() const {
  return GetType() == kDirLayer || GetType() == kMorpherLayer;
}

std::span<Layer* const> Layer::GetChild() const {
  assert(HasChild() && "Layer does not have child");
  return std::span<Layer* const>(_child.data(), _child.size());
}

std::span<Layer*> Layer::GetChild() {
  assert(HasChild() && "Layer does not have child");
  return _child;
}
void Layer::SetLayerData(std::unique_ptr<LayerData> meta_data) {
  _meta_data = std::move(meta_data);
}

Layer::~Layer() {
  for (auto* child : _child) {
    delete child;
  }
  _child.clear();
}
Layer::LayerIterator::LayerIterator(Layer* root, bool front_iter) {
  _root = root;
  if (_root) {
    _stack.push_back({.layer = _root, .depth = 0});
  }
  _front_iter = front_iter;
}
const Layer::LayerIterator::Element& Layer::LayerIterator::get() const {
  return _stack.back();
}
Layer::LayerIterator& Layer::LayerIterator::operator++() {
  if (_stack.empty()) {
    return *this;
  }
  auto current = _stack.back();
  _stack.pop_back();
  if (current.layer->HasChild()) {
    for (int i = 0; i < current.layer->GetChild().size(); ++i) {
      auto idx = 0;
      if (_front_iter) {
        idx = current.layer->GetChild().size() - 1 - i;
      }
      auto* child = current.layer->GetChild()[idx];
      _stack.push_back({.layer = child, .depth = current.depth + 1});
    }
  }

  return *this;
}
bool Layer::LayerIterator::operator!=(const LayerIterator& other) const {
  if (_stack.empty() && other._stack.empty()) {
    return false;
  }
  if (_stack.empty() || other._stack.empty()) {
    return true;
  }
  return get().layer != other.get().layer;
}

}  // namespace editor

namespace editor {
void ImageLayerData::Serialize(nlohmann::json& json) const {
  json["image_id"] = image_id;
  json["is_visible"] = is_visible();
  std::vector<float> tmp_pos(points.size() * 2);
  std::vector<float> tmp_uv(points.size() * 2);
  memcpy(tmp_pos.data(), points.data(), points.size() * 2 * sizeof(float));
  memcpy(tmp_uv.data(), uvs.data(), uvs.size() * 2 * sizeof(float));
  json["points"] = tmp_pos;
  json["uv"] = tmp_uv;
  json["indices"] = indices;
}
void ImageLayerData::Deserialize(const nlohmann::json& json) {
  image_id = json["image_id"].get<int>();
  is_visible = json["is_visible"].get<bool>();
  auto points_array = json["points"].get<std::vector<float>>();
  auto uvs_array = json["uv"].get<std::vector<float>>();
  points.resize(points_array.size() / 2);
  uvs.resize(uvs_array.size() / 2);
  memcpy(points.data(), points_array.data(),
         points_array.size() * sizeof(float));
  memcpy(uvs.data(), uvs_array.data(), uvs_array.size() * sizeof(float));
  indices = json["indices"].get<std::vector<uint32_t>>();
}
}  // namespace editor