#include "layer.h"

#include <any>
#include <memory>

namespace editor {

Layer::Layer() = default;
Layer::Layer(const std::string& layer_name, LayerDataType data_type) {
  _layer_name = layer_name;
  _type = data_type;
}

bool Layer::HasChild() const {
  return _type == kDirLayer || _type == kMorpherLayer;
}

std::span<Layer* const> Layer::GetChild() const {
  assert(HasChild() && "Layer does not have child");
  return std::span<Layer* const>(_child.data(), _child.size());
}

std::span<Layer*> Layer::GetChild() {
  assert(HasChild() && "Layer does not have child");
  return _child;
}
void Layer::SetOwnerLayerData(void* meta_data,
                              const std::function<void(void*)>& deleter) {
  if (_meta_data_deleter) {
    _meta_data_deleter(_meta_data);
  }
  _meta_data = meta_data;
  _meta_data_deleter = deleter;
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