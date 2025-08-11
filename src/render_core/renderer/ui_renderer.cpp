#include "ui_renderer.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include "render_core/vulkan_driver.h"

namespace rdc {

StaticUiResource::StaticUiResource(const std::string &res_name,
                                   const CPUImage *image) {
  _res_name = res_name;
  auto *driver = VulkanDriver::GetSingleton();
  {
    const auto *cpu_image = image;

    VkDeviceSize const size = static_cast<int64_t>(
        cpu_image->width * cpu_image->height * cpu_image->channels);

    VkBuffer staging_buffer;
    VmaAllocation staging_allocation;
    driver->HCreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VMA_MEMORY_USAGE_CPU_ONLY, staging_buffer,
                          staging_allocation);
    void *data;
    vmaMapMemory(driver->GetVmaAllocator(), staging_allocation, &data);
    memcpy(data, cpu_image->data, size);
    vmaUnmapMemory(driver->GetVmaAllocator(), staging_allocation);

    // create vkimage
    VmaAllocationCreateInfo const alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    VkImageCreateInfo const image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent =
            {
                .width = cpu_image->width,
                .height = cpu_image->height,
                .depth = 1,
            },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    AssertVkResult(vmaCreateImage(driver->GetVmaAllocator(), &image_info,
                                  &alloc_info, &_image, &_allocation, nullptr),
                   "Failed to create image");

    VkCommandBuffer single_command_buffer =
        driver->HBeginOneTimeCommandBuffer();

    driver->HTransitionImageLayout(
        single_command_buffer, _image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy const copy_region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageExtent =
            {
                .width = cpu_image->width,
                .height = cpu_image->height,
                .depth = 1,
            },
    };
    vkCmdCopyBufferToImage(single_command_buffer, staging_buffer, _image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);

    driver->HTransitionImageLayout(
        single_command_buffer, _image, VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // create image view
    VkImageViewCreateInfo const image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = _image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };
    AssertVkResult(vkCreateImageView(driver->GetDevice(), &image_view_info,
                                     nullptr, &_image_view),
                   "Failed to create image view");
  }
}

StaticUiResource::~StaticUiResource() {
  auto *driver = VulkanDriver::GetSingleton();
  vkDestroyImageView(driver->GetDevice(), _image_view, nullptr);
  vmaDestroyImage(driver->GetVmaAllocator(), _image, _allocation);
}

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
UiRenderer::UiRenderer() {
  InitImGuiRender();
  auto *driver = VulkanDriver::GetSingleton();
  _ui_sampler = driver->HCreateSimpleSampler();
}
UiRenderer::~UiRenderer() {
  vkDestroySampler(VulkanDriver::GetSingleton()->GetDevice(), _ui_sampler, nullptr);
  ImGui_ImplVulkan_Shutdown();
}
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

void UiRenderer::AddStaticResource(const std::string &res_name,
                                   const CPUImage *image) {
  auto resource = std::make_unique<StaticUiResource>(res_name, image);
  _static_resources[res_name] = std::move(resource);
}
ImTextureID UiRenderer::GetStaticResourceId(const std::string &res_name) {
  if (_static_resources.find(res_name) == _static_resources.end()) {
    return 0;
  }
  return _static_resources[res_name]->GetTextureId();
}

}  // namespace rdc