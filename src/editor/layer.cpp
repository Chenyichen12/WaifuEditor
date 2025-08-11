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
}  // namespace editor