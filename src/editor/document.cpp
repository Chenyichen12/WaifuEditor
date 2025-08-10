#include "document.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <ostream>

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