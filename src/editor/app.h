#ifndef EDITOR_APP_H_
#define EDITOR_APP_H_
#include <memory>
#include "document.h"
#include "gui.h"
#include "render_core/renderer/renderer.h"
namespace editor {
class App {
  void AppInitContext();

  std::unique_ptr<Gui> _gui;
  std::unique_ptr<rdc::ApplicationRenderer> _renderer;
  std::unique_ptr<Document> _current_document;

 public:
  explicit App(int argc, char** argv);
  void OpenDocument(std::unique_ptr<Document> doc);

  void Exec();
  ~App();
};
}  // namespace editor
#endif  // EDITOR_APP_H_
