#ifndef EDITOR_DOCUMENT_H_
#define EDITOR_DOCUMENT_H_
#include <memory>
#include <string>

#include "tools.hpp"

namespace editor {
class Document {
  std::string _file_path;

 public:
  static std::unique_ptr<Document> LoadFromPath(const std::string &path);
};

class EditorConfig : public NoCopyable {
 private:
  EditorConfig();
 public:
  static EditorConfig *GetInstance() {
    static EditorConfig instance;
    return &instance;
  }

  Property<int> LastTimeWinX = 0;
  Property<int> LastTimeWinY = 0;
  Property<int> LastTimeWinWidth = 800;
  Property<int> LastTimeWinHeight = 600;

  void SaveConfig() const;
};
}  // namespace editor

#endif  // EDITOR_DOCUMENT_H_