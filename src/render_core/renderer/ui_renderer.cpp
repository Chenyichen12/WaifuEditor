#include "ui_renderer.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "render_core/vulkan_driver.h"


namespace rdc {
void UiRenderer::RecordUiCommandBuffer(VkCommandBuffer command_buffer) {
  ImGui::Render();
  auto *draw_data = ImGui::GetDrawData();
  if (draw_data == nullptr) {
    return;
  }
  auto *driver = VulkanDriver::GetSingleton();
  {
    VkRenderingAttachmentInfo att_info = {};
    att_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att_info.imageView = _render_target_view;
    att_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    att_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att_info.clearValue = {.color = {0.8f, 0.8f, 0.8f, 1.0f}};
    att_info.resolveMode = VK_RESOLVE_MODE_NONE;

    // render_info
    VkRenderingInfo const render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = driver->GetSwapchainExtent(),
            },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &att_info,
    };
    vkCmdBeginRenderingKHR(command_buffer, &render_info);
  }
  {
    ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
  }
  {
    vkCmdEndRenderingKHR(command_buffer);
  }
}
UiRenderer::UiRenderer() { InitImGuiRender(); }
UiRenderer::~UiRenderer() { ImGui_ImplVulkan_Shutdown(); }
void UiRenderer::InitImGuiRender() {
  auto *ctx = ImGui::GetCurrentContext();
  if (ctx == nullptr) {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
  }
  ImGui_ImplVulkan_InitInfo init_info = {};
  auto *driver = VulkanDriver::GetSingleton();
  init_info.Instance = driver->GetInstance();
  init_info.ApiVersion = VK_API_VERSION_1_2;
  init_info.PhysicalDevice = driver->GetPhysicalDevice();
  init_info.Device = driver->GetDevice();
  init_info.QueueFamily = driver->GetGraphicsQueueFamilyIndex();
  init_info.Queue = driver->GetGraphicsQueue();
  init_info.DescriptorPool = driver->GetDescriptorPool();
  init_info.MinImageCount = driver->GetSurfaceCapabilities().minImageCount;
  init_info.ImageCount = driver->GetSwapchainImages().size();
  init_info.CheckVkResultFn = [](VkResult err) { AssertVkResult(err); };
  init_info.UseDynamicRendering = true;

  VkPipelineRenderingCreateInfoKHR const pipeline_rendering_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
      .pNext = nullptr,
      .viewMask = 0,
      .colorAttachmentCount = 1,
      .pColorAttachmentFormats = &driver->GetSwapchainFormat()};
  init_info.PipelineRenderingCreateInfo = pipeline_rendering_info;

  ImGui_ImplVulkan_Init(&init_info);

  // only build font after build vulkan
  ImGui::GetIO().Fonts->Build();
}

}  // namespace rdc