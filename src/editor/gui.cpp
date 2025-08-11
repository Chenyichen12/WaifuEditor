#include "gui.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <portable-file-dialogs.h>

#include <iostream>
#include <nlohmann/json.hpp>

#include "document.h"
#include "render_core/vulkan_driver.h"
#include "tools.hpp"

namespace editor {

void Gui::WindowResizeCallback(GLFWwindow *window, int width, int height) {
  auto *gui = static_cast<Gui *>(glfwGetWindowUserPointer(window));
  if (gui) {
    gui->WindowResizeSignal(width, height);
  };
  auto *driver = rdc::VulkanDriver::GetSingleton();
  if (driver) {
    driver->MarkSwapchainInvalid();
  }
}

Gui::Gui() {
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  int win_x = 20;
  int win_y = 20;
  int width = 800;
  int height = 600;
  bool is_maximized = false;
  {
    EditorConfig *config = EditorConfig::GetInstance();
    win_x = config->LastTimeWinX();
    win_y = config->LastTimeWinY();
    width = config->LastTimeWinWidth();
    height = config->LastTimeWinHeight();
  }

  _window = glfwCreateWindow(width, height, "WaifuStudio", nullptr, nullptr);
  glfwSetWindowSizeLimits(_window, 800, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);
  glfwSetWindowPos(_window, win_x, win_y);
  if (is_maximized) {
    glfwMaximizeWindow(_window);
  }

  if (!_window) {
    std::cerr << "fail to create window\n";
    std::abort();
  }

  // set resize callback
  glfwSetWindowUserPointer(_window, this);
  glfwSetWindowSizeCallback(_window, WindowResizeCallback);
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &imgui_io = ImGui::GetIO();
    (void)imgui_io;
    imgui_io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    imgui_io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable Docking

    ImGui_ImplGlfw_InitForVulkan(_window, true);

    auto *font =
        imgui_io.Fonts->AddFontFromFileTTF("res/souce_han_normal.otf", 30.0f);
    imgui_io.FontDefault = font;
  }
}

Gui::~Gui() {
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  EditorConfig *config = EditorConfig::GetInstance();
  int win_x;
  int win_y;
  int width;
  int height;

  glfwGetWindowPos(_window, &win_x, &win_y);
  glfwGetWindowSize(_window, &width, &height);
  config->LastTimeWinX = win_x;
  config->LastTimeWinY = win_y;
  config->LastTimeWinWidth = width;
  config->LastTimeWinHeight = height;

  if (_window) {
    glfwDestroyWindow(_window);
    _window = nullptr;
  }
}

void Gui::TickGui() {
  ImGui_ImplGlfw_NewFrame();
  ImGui_ImplVulkan_NewFrame();
  ImGui::NewFrame();

  // main window
  {
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;  // 不渲染背景

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    const char *main_window_name = WaifuTr("Main Window");
    // no padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin(main_window_name, nullptr, window_flags);
    ImGui::PopStyleVar();

    
    // meau
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu(WaifuTr("File"))) {
        if (ImGui::MenuItem(WaifuTr("Open"), "Ctrl+O")) {
          // open file dialog
          auto file = pfd::open_file(WaifuTr("Open Project"), "",
                                     {"Project File", "*.json"});
          auto result = file.result();
          if (!result.empty()) {
            DocumentOpenSignal(result[0]);
          }
        }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID(main_window_name);
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                     ImGuiDockNodeFlags_PassthruCentralNode);
    {
      ImGui::Begin("Layer panel");

      ImGui::End();
    }
    ImGui::End();
  }
  {
    // ImGui::ShowMetricsWindow();
  }
}
void Gui::GetWindowSize(int &width, int &height) const {
  if (_window) {
    glfwGetFramebufferSize(_window, &width, &height);
  } else {
    width = 0;
    height = 0;
  }
}

VkResult Gui::CreateVulkanSurface(VkInstance instance,
                                  VkSurfaceKHR &surface) const {
  if (!_window) {
    return VK_ERROR_INITIALIZATION_FAILED;
  }
  return glfwCreateWindowSurface(instance, _window, nullptr, &surface);
}

}  // namespace editor