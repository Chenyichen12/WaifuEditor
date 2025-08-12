#include "document.h"

#include <any>
#include <cstddef>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

#include "editor/types.hpp"
#include "layer.h"

namespace editor {
std::unique_ptr<Document> Document::LoadFromPath(const std::string &path) {
  return nullptr;
}
std::unique_ptr<Document> Document::LoadFromLayerConfig(
    const std::string &config_path) {
  auto config_dir = std::filesystem::path(config_path).parent_path();
  auto result = std::make_unique<Document>();
  // root

  result->_file_path = config_path;
  {
    auto root = std::make_unique<Layer>();
    root->SetType(Layer::kDirLayer);
    root->SetLayerName("Root");
    result->_doc_root_layer = std::move(root);
  }

  nlohmann::json layer_config;
  {
    std::ifstream config_file(config_path);
    assert(config_file.is_open() && "Failed to open config file");
    config_file >> layer_config;
  }

  for (const auto &layer : layer_config["layers"]) {
    std::string file_path = layer["path"];
    file_path = (config_dir / file_path).string();

    std::unique_ptr<CPUImage> image = std::make_unique<CPUImage>();
    image->LoadFromFile(file_path);

    // doc layer build
    std::string layer_name = layer["name"];
    auto *doc_layer = new Layer(layer_name, Layer::kImageLayer);
    auto *meta_data = new ImageLayerData();
    meta_data->image = image.get();
    meta_data->is_visible = true;

    auto vertex_struct = layer["vertices"];
    auto position_array = vertex_struct["position"].get<std::vector<float>>();
    auto uv_array = vertex_struct["uv"].get<std::vector<float>>();
    auto index_array = vertex_struct["index"].get<std::vector<uint32_t>>();
    assert(position_array.size() % 2 == 0 &&
           "Position array size must be even");
    assert(uv_array.size() % 2 == 0 && "UV array size must be even");

    for (size_t i = 0; i < position_array.size() / 2; ++i) {
      glm::vec2 pos{position_array[i * 2], position_array[(i * 2) + 1]};
      glm::vec2 pos_uv{uv_array[i * 2], uv_array[(i * 2) + 1]};
      meta_data->points.push_back(pos);
      meta_data->uvs.push_back(pos_uv);
    }
    meta_data->indices = std::move(index_array);
    doc_layer->SetOwnerLayerData(meta_data, [](void *ptr) {
      delete static_cast<ImageLayerData *>(ptr);
    });

    result->_doc_root_layer->AddChild(doc_layer);
    result->_images_container.push_back(std::move(image));
  }
  {
    result->_canvas_size.x = layer_config["canvas"]["width"].get<int>();
    result->_canvas_size.y = layer_config["canvas"]["height"].get<int>();
  }
  return result;
}
Document::Document() = default;
Document::~Document() = default;
}  // namespace editor
namespace editor {
EditorConfig::EditorConfig() {
  std::ifstream config_file("editor_config.json");
  if (config_file.is_open()) {
    nlohmann::json config_json;
    config_file >> config_json;
    LastTimeWinX = config_json["last_time_win_x"].get<int>();
    LastTimeWinY = config_json["last_time_win_y"].get<int>();
    LastTimeWinWidth = config_json["last_time_win_width"].get<int>();
    LastTimeWinHeight = config_json["last_time_win_height"].get<int>();
    LastTimeDocumentPath =
        config_json["last_time_document_path"].get<std::string>();
  }
}

void EditorConfig::SaveConfig() const {
  std::ofstream config_file("editor_config.json");
  if (config_file.is_open()) {
    nlohmann::json config_json;
    config_json["last_time_win_x"] = LastTimeWinX();
    config_json["last_time_win_y"] = LastTimeWinY();
    config_json["last_time_win_width"] = LastTimeWinWidth();
    config_json["last_time_win_height"] = LastTimeWinHeight();
    config_json["last_time_document_path"] = LastTimeDocumentPath();
    config_file << config_json.dump(4);
  }
}
}  // namespace editor