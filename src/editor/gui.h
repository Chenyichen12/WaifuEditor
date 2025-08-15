#ifndef EDITOR_GUI_H_
#define EDITOR_GUI_H_
#include <string>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <sigslot/signal.hpp>


namespace editor {
class Gui {
  GLFWwindow *_window = nullptr;
  static void WindowResizeCallback(GLFWwindow *window, int width, int height);
  static void WindowPosCallback(GLFWwindow *window, int x, int y);

 public:
  Gui();
  ~Gui();
  void TickGui();

  GLFWwindow *GetWindow() const { return _window; }
  void GetWindowSize(int &width, int &height) const;
  VkResult CreateVulkanSurface(VkInstance instance,
                               VkSurfaceKHR &surface) const;
  static std::string OpenSaveDialog();

  // signals
  sigslot::signal<int, int> WindowResizeSignal;
  sigslot::signal<const std::string&> DocumentOpenSignal;
  sigslot::signal<const std::string&> DocumentLoadPsdSignal;
  sigslot::signal<> DocumentSaveSignal;
};
}  // namespace editor

#endif  // EDITOR_GUI_H_
