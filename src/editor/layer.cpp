#include "layer.h"

#include <memory>


namespace editor {

Layer::Layer() : _meta_data(nullptr, [](void*) {}) {
    
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

void Layer::SetLayerData(std::unique_ptr<void*, void (*)(void*)> data) {
  _meta_data = std::move(data);
}

Layer::~Layer() {
  for (auto* child : _child) {
    delete child;
  }
  _child.clear();
}
}  // namespace editor