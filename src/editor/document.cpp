#include "document.h"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include "editor/types.hpp"
#include "layer.h"

namespace editor {

std::unique_ptr<Document> Document::LoadFromLayerConfig(
    const std::string &config_path) {
  auto config_dir = std::filesystem::path(config_path).parent_path();
  auto result = std::make_unique<Document>();
  // root
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
    auto vertex_struct = layer["vertices"];
    auto position_array = vertex_struct["position"].get<std::vector<float>>();
    auto uv_array = vertex_struct["uv"].get<std::vector<float>>();
    auto index_array = vertex_struct["index"].get<std::vector<uint32_t>>();

    std::string file_path = layer["path"];
    file_path = (config_dir / file_path).string();

    std::unique_ptr<CPUImage> image = std::make_unique<CPUImage>();
    image->LoadFromFile(file_path);
    // layer_data->image = image.get();
    auto *doc_layer = new Layer();
    doc_layer->SetLayerName(layer["name"]);
    doc_layer->SetType(Layer::kImageLayer);
    // doc_layer->SetLayerData()

    // result->_doc_root_layer->AddChild(doc_layer);

  }
  return result;
}
Document::~Document() = default;

EditorConfig::EditorConfig() {
  std::ifstream config_file("editor_config.json");
  if (config_file.is_open()) {
    nlohmann::json config_json;
    config_file >> config_json;
    LastTimeWinX = config_json["last_time_win_x"].get<int>();
    LastTimeWinY = config_json["last_time_win_y"].get<int>();
    LastTimeWinWidth = config_json["last_time_win_width"].get<int>();
    LastTimeWinHeight = config_json["last_time_win_height"].get<int>();
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
    config_file << config_json.dump(4);
  }
}
}  // namespace editor