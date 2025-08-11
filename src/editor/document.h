#ifndef EDITOR_DOCUMENT_H_
#define EDITOR_DOCUMENT_H_
#include <memory>
#include <string>

#include "tools.hpp"
#include "layer.h"
namespace editor {


class Document {
  std::string _file_path;
  std::unique_ptr<Layer> _doc_root_layer;

  std::vector<std::unique_ptr<CPUImage>> _images_container;

 public:
  static std::unique_ptr<Document> LoadFromPath(const std::string &path);
  static std::unique_ptr<Document> LoadFromLayerConfig(const std::string& config_path);
  Document();
  ~Document();
};

class EditorConfig : public NoCopyable {
 private:
  EditorConfig();
 public:
  static EditorConfig *GetInstance() {
    static EditorConfig instance;
    return &instance;
  }

  Property<int> LastTimeWinX {0};
  Property<int> LastTimeWinY {0};
  Property<int> LastTimeWinWidth {800};
  Property<int> LastTimeWinHeight {600};

  void SaveConfig() const;
};
}  // namespace editor

#endif  // EDITOR_DOCUMENT_H_