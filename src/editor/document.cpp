#include "document.h"

#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stack>

#include "editor/types.hpp"
#include "layer.h"

namespace editor {
std::unique_ptr<Document> Document::LoadFromPath(const std::string &path) {
  std::unique_ptr<Document> result = std::make_unique<Document>();
  result->_file_path = path;
  std::ifstream proj_file(path);
  if (!proj_file.is_open()) {
    return nullptr;
  }
  nlohmann::json proj_json;
  proj_file >> proj_json;

  // image
  {
    for (const auto &image_json : proj_json.at("images")) {
      DocumentImage doc_image;
      doc_image.image_id = image_json.at("id").get<int>();
      doc_image.rel_path = image_json.at("rel_path").get<std::string>();
      // load image data
      std::filesystem::path image_path =
          std::filesystem::path(result->_file_path).parent_path();
      image_path = image_path / doc_image.rel_path;
      auto image = std::make_unique<CPUImage>();
      image->LoadFromFile(image_path.string());
      if (!image->IsValid()) {
        return nullptr;  // Failed to load image
      }
      doc_image.image = std::move(image);
      result->_images_container.push_back(std::move(doc_image));
    }
  }

  // board
  {
    result->_canvas_size.x = proj_json.at("board").at("width");
    result->_canvas_size.y = proj_json.at("board").at("height");
    {
      auto root =
          std::make_unique<Layer>("Root", std::make_unique<DirLayerData>());
      std::stack<std::pair<Layer *, int>> layer_stack;
      std::pair<Layer *, int> pre_layer = {root.get(), -1};

      for (const auto &layer : proj_json.at("board").at("layer")) {
        auto new_layer = std::make_unique<Layer>();
        int const depth = layer.at("depth");
        new_layer->SetLayerName(layer.at("name").get<std::string>());
        auto type = static_cast<LayerDataType>(layer.at("type").get<int>());
        auto meta = layer.at("meta");
        auto new_layer_data = LayerData::Create(type, meta);
        if (new_layer_data->Type() == kImageLayer) {
          auto image_data = static_cast<ImageLayerData *>(new_layer_data.get());
          image_data->image =
              result->_images_container[image_data->image_id].image.get();
        }

        new_layer->SetLayerData(std::move(new_layer_data));
        // add child
        if (pre_layer.second < depth) {
          // deeper
          layer_stack.push(pre_layer);
          auto *new_layer_ptr = new_layer.release();
          pre_layer.first->AddChild(new_layer_ptr);
          pre_layer = {new_layer_ptr, depth};

        } else if (pre_layer.second == depth) {
          // same depth
          auto *new_layer_ptr = new_layer.release();
          layer_stack.top().first->AddChild(new_layer_ptr);
          pre_layer.first = new_layer_ptr;
        } else if (pre_layer.second > depth) {
          // shallower
          while (!layer_stack.empty() && pre_layer.second > depth) {
            pre_layer = layer_stack.top();
            layer_stack.pop();
          }
          auto *new_layer_ptr = new_layer.release();
          layer_stack.top().first->AddChild(new_layer_ptr);
          pre_layer = {new_layer_ptr, depth};
        }
      }
      // set root
      result->_doc_root_layer = std::move(root);
    }
  }
  return result;
}
std::unique_ptr<Document> Document::LoadFromLayerConfig(
    const std::string &config_path) {
  try {
    auto config_dir = std::filesystem::path(config_path).parent_path();
    auto result = std::make_unique<Document>();
    // root

    // result->_file_path = config_path;
    {
      auto root =
          std::make_unique<Layer>("Root", std::make_unique<DirLayerData>());
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
      std::string layer_name = layer.at("name");
      auto *doc_layer = new Layer();
      doc_layer->SetLayerName(layer_name);
      auto meta_data = std::make_unique<ImageLayerData>();
      meta_data->image = image.get();
      meta_data->image_id = result->_images_container.size();
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
      doc_layer->SetLayerData(std::move(meta_data));

      result->_doc_root_layer->AddChild(doc_layer);
      // result->_images_container.push_back(std::move(image));

      DocumentImage doc_image;
      doc_image.image = std::move(image);
      doc_image.image_id = result->_images_container.size();
      doc_image.rel_path =
          std::filesystem::relative(file_path, config_dir).string();
      result->_images_container.push_back(std::move(doc_image));
    }
    {
      result->_canvas_size.x = layer_config["canvas"]["width"].get<int>();
      result->_canvas_size.y = layer_config["canvas"]["height"].get<int>();
    }

    return result;
  } catch (...) {
    std::cerr << "Failed to load config file \n";
    return nullptr;
  }
}
bool Document::SaveProject() const {
  // if not create file, create it
  std::ofstream proj_file(_file_path);
  if (!proj_file.is_open()) {
    return false;
  }
  nlohmann::json proj_config;

  // image part
  {
    for (const auto &doc_image : _images_container) {
      nlohmann::json image_json;
      image_json["id"] = doc_image.image_id;
      image_json["rel_path"] = doc_image.rel_path;
      // save image data
      std::filesystem::path path =
          std::filesystem::path(_file_path).parent_path();
      path = path / doc_image.rel_path;
      if (!std::filesystem::exists(path)) {
        doc_image.image->SaveToFile(path.string());
      }

      proj_config["images"].push_back(image_json);
    }
  }

  // board part
  {
    proj_config["board"]["width"] = static_cast<int>(_canvas_size.x);
    proj_config["board"]["height"] = static_cast<int>(_canvas_size.y);

    for (auto it = _doc_root_layer->BeginFrontIter();
         it != _doc_root_layer->EndFrontIter(); ++it) {
      nlohmann::json layer_json;
      auto element = it.get();
      int depth = element.depth;
      layer_json["name"] = element.layer->GetLayerName();
      layer_json["depth"] = depth;
      layer_json["type"] = element.layer->GetType();
      layer_json["meta"] = nlohmann::json::object();
      element.layer->GetLayerData()->Serialize(layer_json["meta"]);
      proj_config["board"]["layer"].push_back(layer_json);
    }
  }
  proj_file << proj_config.dump(4);
  return true;
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
    SafeGetProperty(config_json, "last_time_win_x", LastTimeWinX);
    SafeGetProperty(config_json, "last_time_win_y", LastTimeWinY);
    SafeGetProperty(config_json, "last_time_win_width", LastTimeWinWidth);
    SafeGetProperty(config_json, "last_time_win_height", LastTimeWinHeight);
    SafeGetProperty(config_json, "last_time_document_path",
                    LastTimeDocumentPath);
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