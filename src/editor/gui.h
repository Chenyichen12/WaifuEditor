#ifndef EDITOR_GUI_H_
#define EDITOR_GUI_H_
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

  // signals
  sigslot::signal<int, int> WindowResizeSignal;
};
}  // namespace editor

#endif  // EDITOR_GUI_H_
