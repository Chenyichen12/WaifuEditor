#include "renderer.h"
#include <memory>
#include <vector>

#include "vulkan/vulkan_core.h"
#include "render_core/vulkan_driver.h"

namespace rdc {
ApplicationRenderer::ApplicationRenderer() {
  _model_renderer = std::make_unique<ModelRenderer>();
  _ui_renderer = std::make_unique<UiRenderer>();
  auto *driver = VulkanDriver::GetSingleton();
  {
    VkCommandBufferAllocateInfo const alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = driver->GetCommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    AssertVkResult(vkAllocateCommandBuffers(driver->GetDevice(), &alloc_info,
                                            &_app_command_buffer),
                   "Failed to allocate command buffer");
  }
  {
    VkSemaphoreCreateInfo constexpr semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    AssertVkResult(
        vkCreateSemaphore(driver->GetDevice(), &semaphore_info, nullptr,
                          &_swapchain_image_available_semaphore),
        "Failed to create swapchain image available semaphore");
    AssertVkResult(vkCreateSemaphore(driver->GetDevice(), &semaphore_info,
                                     nullptr, &_graphics_finished_semaphore),
                   "Failed to create graphics finished semaphore");
  }
  {
    _app_resource_manager = std::make_unique<RenderResourceManager>();
  }
}
ApplicationRenderer::~ApplicationRenderer() {
  auto* driver = VulkanDriver::GetSingleton();
  vkDeviceWaitIdle(driver->GetDevice());
  vkDestroySemaphore(driver->GetDevice(), _swapchain_image_available_semaphore,
                     nullptr);
  vkDestroySemaphore(driver->GetDevice(), _graphics_finished_semaphore,
                     nullptr);
  vkFreeCommandBuffers(driver->GetDevice(), driver->GetCommandPool(), 1,
                       &_app_command_buffer);
}
void ApplicationRenderer::SetWindowSize(int width, int height) {
  _window_height = height;
  _window_width = width;
}
void ApplicationRenderer::Render() {
  auto *driver = VulkanDriver::GetSingleton();
  if (!driver->IsSwapchainValid()) {
    driver->RecreateSwapchain({static_cast<uint32_t>(_window_width),
                               static_cast<uint32_t>(_window_height)});
  }
  vkQueueWaitIdle(driver->GetGraphicsQueue());

  // acquire image
  uint32_t index = 0;
  auto acquire_result = driver->AcquireSwapchainNextImage(
      _swapchain_image_available_semaphore, VK_NULL_HANDLE, index);
  if (acquire_result != VK_SUCCESS) {
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR ||
        acquire_result == VK_SUBOPTIMAL_KHR) {
      driver->MarkSwapchainInvalid();
      return;
    }
    AssertVkResult(acquire_result, "Failed to acquire swapchain image");
  }

  auto *target_image = driver->GetSwapchainImages()[index];
  auto *target_image_view = driver->GetSwapchainImageViews()[index];

  constexpr VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
  };

  // prepare
  _model_renderer->PrepareRender();

  vkResetCommandBuffer(_app_command_buffer, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  AssertVkResult(vkBeginCommandBuffer(_app_command_buffer, &begin_info),
                 "Failed to begin command buffer");

  // model render
  driver->HTransitionImageLayout(_app_command_buffer, target_image, 0,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  _model_renderer->SetTargetView(target_image_view);
  _model_renderer->RecordCommandBuffer(_app_command_buffer);

  _ui_renderer->SetRenderTargetView(target_image_view);
  _ui_renderer->RecordUiCommandBuffer(_app_command_buffer);

  driver->HTransitionImageLayout(_app_command_buffer, target_image,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  // end model render

  vkEndCommandBuffer(_app_command_buffer);

  VkPipelineStageFlags const pipeline_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };
  VkSubmitInfo const submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &_swapchain_image_available_semaphore,
      .pWaitDstStageMask = pipeline_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &_app_command_buffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &_graphics_finished_semaphore,
  };
  vkQueueSubmit(driver->GetGraphicsQueue(), 1, &submit_info, VK_NULL_HANDLE);

  VkPresentInfoKHR const present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = nullptr,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &_graphics_finished_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &driver->GetSwapchain(),
      .pImageIndices = &index,
      .pResults = nullptr,
  };
  auto result = vkQueuePresentKHR(driver->GetPresentQueue(), &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    driver->MarkSwapchainInvalid();
  } else if (result != VK_SUCCESS) {
    AssertVkResult(result, "Failed to present swapchain image");
  }
}

}  // namespace rdc