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
LayerIterator::LayerIterator(Layer* root) {
  _root = root;
  if (!_root) {
    return;
  }
  _stack.push_back({.layer = _root, .depth = 0});
}
const LayerIterator::Element& LayerIterator::operator*() const {
  return _stack.back();
}
LayerIterator& LayerIterator::operator++() {
  if (_stack.empty()) {
    return *this;
  }
  auto& current = _stack.back();
  _stack.pop_back();
  if (current.layer->HasChild()) {
    for (auto* child : current.layer->GetChild()) {
      _stack.push_back({.layer = child, .depth = current.depth + 1});
    }
  }

  return *this;
}
bool LayerIterator::operator!=(const LayerIterator& other) const {
  return !_stack.empty();
}
LayerIterator LayerIterator::begin() { return LayerIterator(_root); }
LayerIterator LayerIterator::end() {
  return LayerIterator(nullptr);
}
}  // namespace editor