#include "renderer.h"

#include <array>
#include <cassert>
#include <memory>
#include <vector>

#include "canvas_sd.gen.h"
#include "vulkan/vulkan_core.h"
#include "vulkan_driver.h"

namespace {

void SetVertexInput(VkCommandBuffer cmd) {
  VkVertexInputBindingDescription2EXT const bindings = {
      .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
      .binding = 0,
      .stride = sizeof(rdc::ModelVertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
      .divisor = 1,
  };

  // VkVertexInputAttributeDescription attribute[2];
  std::array<VkVertexInputAttributeDescription2EXT, 2> attributes;
  attributes[0] = {
      .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
      .location = 0,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(rdc::ModelVertex, position),
  };

  attributes[1] = {
      .sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
      .location = 1,
      .binding = 0,
      .format = VK_FORMAT_R32G32_SFLOAT,
      .offset = offsetof(rdc::ModelVertex, uv),
  };

  vkCmdSetVertexInputEXT(cmd, 1, &bindings,
                         static_cast<uint32_t>(attributes.size()),
                         attributes.data());
}

}  // namespace

namespace rdc {

std::unique_ptr<Layer2dResource> Layer2dResource::CreateFromImage(
    const ImageConfig &config) {
  assert(VulkanDriver::GetSingleton() != nullptr &&
         "VulkanDriver is not initialized");
  auto *driver = VulkanDriver::GetSingleton();

  auto result = std::unique_ptr<Layer2dResource>(new Layer2dResource());

  // upload data
  auto *cpu_image = config.pimage;

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
      .format = config.format,
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

  AssertVkResult(
      vmaCreateImage(driver->GetVmaAllocator(), &image_info, &alloc_info,
                     &result->_image, &result->_allocation, nullptr),
      "Failed to create image");

  VkCommandBuffer single_command_buffer = driver->HBeginOneTimeCommandBuffer();

  driver->HTransitionImageLayout(
      single_command_buffer, result->_image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
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
  vkCmdCopyBufferToImage(single_command_buffer, staging_buffer, result->_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  driver->HTransitionImageLayout(
      single_command_buffer, result->_image, VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  result->_vertices =
      std::vector<ModelVertex>(config.vertices.begin(), config.vertices.end());
  result->_indices =
      std::vector<uint32_t>(config.indices.begin(), config.indices.end());

  // create vertex buffer
  driver->HCreateBuffer(
      sizeof(ModelVertex) * result->_vertices.size(),
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU, result->_vertex_buffer._buffer,
      result->_vertex_buffer._allocation);

  // copy vertex data
  vmaMapMemory(driver->GetVmaAllocator(), result->_vertex_buffer._allocation,
               &data);

  memcpy(data, result->_vertices.data(),
         sizeof(ModelVertex) * result->_vertices.size());
  vmaUnmapMemory(driver->GetVmaAllocator(), result->_vertex_buffer._allocation);

  // create index buffer
  driver->HCreateBuffer(
      sizeof(uint32_t) * result->_indices.size(),
      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VMA_MEMORY_USAGE_CPU_TO_GPU, result->_index_buffer._buffer,
      result->_index_buffer._allocation);
  vmaMapMemory(driver->GetVmaAllocator(), result->_index_buffer._allocation,
               &data);
  memcpy(data, result->_indices.data(),
         sizeof(uint32_t) * result->_indices.size());
  vmaUnmapMemory(driver->GetVmaAllocator(), result->_index_buffer._allocation);

  driver->HEndOneTimeCommandBuffer(single_command_buffer,
                                   driver->GetGraphicsQueue());

  vmaDestroyBuffer(driver->GetVmaAllocator(), staging_buffer,
                   staging_allocation);

  // create image view
  VkImageViewCreateInfo const image_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = result->_image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = config.format,
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
                                   nullptr, &result->_image_view),
                 "Failed to create image view");

  return result;
}

Layer2dResource::~Layer2dResource() {
  auto *driver = VulkanDriver::GetSingleton();
  vmaDestroyImage(driver->GetVmaAllocator(), _image, _allocation);
  vkDestroyImageView(driver->GetDevice(), _image_view, nullptr);
  vmaDestroyBuffer(driver->GetVmaAllocator(), _vertex_buffer._buffer,
                   _vertex_buffer._allocation);
  vmaDestroyBuffer(driver->GetVmaAllocator(), _index_buffer._buffer,
                   _index_buffer._allocation);
}

ModelRenderer::ModelRenderer() {
  const auto *driver = VulkanDriver::GetSingleton();
  assert(driver != nullptr && "driver isn't init");
  {
    _sampler = driver->HCreateSimpleSampler();
  }
  {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.push_back({
        .binding = shader_gen::canvas_sd::ubo.binding,
        .descriptorType = shader_gen::canvas_sd::ubo.desc_type,
        .descriptorCount = 1,
        .stageFlags = shader_gen::canvas_sd::ubo.stages,
    });
    bindings.push_back({
        .binding = shader_gen::canvas_sd::main_tex.binding,
        .descriptorType = shader_gen::canvas_sd::main_tex.desc_type,
        .descriptorCount = 1,
        .stageFlags = shader_gen::canvas_sd::main_tex.stages,
    });

    VkDescriptorSetLayoutCreateInfo const set0_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    vkCreateDescriptorSetLayout(driver->GetDevice(), &set0_info, nullptr,
                                &_descriptor_set_layout);

    // pipeline layout
    VkPipelineLayoutCreateInfo const pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &_descriptor_set_layout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    AssertVkResult(
        vkCreatePipelineLayout(driver->GetDevice(), &pipeline_layout_info,
                               nullptr, &_pipeline_layout),
        "Failed to create pipeline layout");
  }
  {
    VkDeviceSize size = sizeof(shader_gen::canvas_sd::UniformBufferObject);
    driver->HCreateBuffer(
        size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU, _ubo_buffer.buffer,
        _ubo_buffer.allocation);
  }
  {
    // shader objects
    _vertex_shader.stage_flag = VK_SHADER_STAGE_VERTEX_BIT;
    _fragment_shader.stage_flag = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkShaderCreateInfoEXT shader_create_infos[2];
    VkShaderCreateInfoEXT &vert_shader_create_info = shader_create_infos[0];
    vert_shader_create_info = {};
    vert_shader_create_info.sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    vert_shader_create_info.pNext = nullptr;
    vert_shader_create_info.pName = "main";
    vert_shader_create_info.flags = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    vert_shader_create_info.stage = _vertex_shader.stage_flag;
    vert_shader_create_info.nextStage = _fragment_shader.stage_flag;
    vert_shader_create_info.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    vert_shader_create_info.codeSize =
        sizeof(shader_gen::canvas_sd::vertex_spv);
    vert_shader_create_info.pCode = shader_gen::canvas_sd::vertex_spv;
    vert_shader_create_info.setLayoutCount = 1;
    vert_shader_create_info.pSetLayouts = &_descriptor_set_layout;

    VkShaderCreateInfoEXT &frag_shader_create_info = shader_create_infos[1];
    frag_shader_create_info = vert_shader_create_info;
    frag_shader_create_info.stage = _fragment_shader.stage_flag;
    frag_shader_create_info.nextStage = 0;
    frag_shader_create_info.codeSize =
        sizeof(shader_gen::canvas_sd::fragment_spv);
    frag_shader_create_info.pCode = shader_gen::canvas_sd::fragment_spv;

    VkShaderEXT shader_exts[2];

    AssertVkResult(vkCreateShadersEXT(
        driver->GetDevice(), 2, shader_create_infos, nullptr, shader_exts));
    _vertex_shader.shader = shader_exts[0];
    _fragment_shader.shader = shader_exts[1];
  }
}

void ModelRenderer::AutoCenterCanvas() {
  auto width_scale = static_cast<float>(_region.width) / _canvas_width;
  auto height_scale = static_cast<float>(_region.height) / _canvas_height;
  auto canvas_2_region_scale = std::min(width_scale, height_scale);
  float canvas_x_offset =
      (_region.width - _canvas_width * canvas_2_region_scale) / 2.0f;
  float canvas_y_offset =
      (_region.height - _canvas_height * canvas_2_region_scale) / 2.0f;
  _canvas_scale = canvas_2_region_scale;
  _canvas_offset.x = canvas_x_offset;
  _canvas_offset.y = canvas_y_offset;
}
void ModelRenderer::UpdateUniform() {
  const auto driver = VulkanDriver::GetSingleton();
  void *data;
  vmaMapMemory(driver->GetVmaAllocator(), _ubo_buffer.allocation, &data);
  shader_gen::canvas_sd::UniformBufferObject ubo = {};
  ubo.region_scale = _canvas_scale;
  ubo.screen_size = glm::vec2(static_cast<float>(_region.width),
                              static_cast<float>(_region.height));
  ubo.region_offset = _canvas_offset;
  memcpy(data, &ubo, sizeof(ubo));
  vmaUnmapMemory(driver->GetVmaAllocator(), _ubo_buffer.allocation);
}
void ModelRenderer::RecordCommandBuffer(VkCommandBuffer command_buffer) {
  // begin record command buffer
  UpdateUniform();
  auto driver = VulkanDriver::GetSingleton();
  {
    VkRenderingAttachmentInfo att_info = {};
    att_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    att_info.imageView = _render_target_view;
    att_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
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
    vkCmdSetCullModeEXT(command_buffer, VK_CULL_MODE_NONE);
    vkCmdSetDepthTestEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetDepthWriteEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetRasterizerDiscardEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetStencilTestEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetDepthBiasEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetRasterizationSamplesEXT(command_buffer, VK_SAMPLE_COUNT_1_BIT);
    constexpr VkSampleMask mask = ~0u;
    vkCmdSetSampleMaskEXT(command_buffer, VK_SAMPLE_COUNT_1_BIT, &mask);
    vkCmdSetAlphaToOneEnableEXT(command_buffer, VK_FALSE);
    vkCmdSetAlphaToCoverageEnableEXT(command_buffer, VK_FALSE);

    vkCmdSetPolygonModeEXT(command_buffer, VK_POLYGON_MODE_FILL);
    vkCmdSetPrimitiveTopologyEXT(command_buffer,
                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkCmdSetPrimitiveRestartEnableEXT(command_buffer, VK_FALSE);

    constexpr VkBool32 blend_enable = VK_TRUE;
    vkCmdSetColorBlendEnableEXT(command_buffer, 0 /* firstAttachment */,
                                1 /* count */, &blend_enable);
    VkColorBlendEquationEXT constexpr blend_equation = {
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };
    vkCmdSetColorBlendEquationEXT(command_buffer, 0 /* firstAttachment */,
                                  1 /* count */, &blend_equation);
    // blend
    VkPipelineColorBlendAttachmentState color_blend_attachment = {};

    const VkColorComponentFlags color_mask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    vkCmdSetColorWriteMaskEXT(command_buffer, 0 /* firstAttachment */,
                              1 /* count */, &color_mask);

    const VkViewport viewport = {static_cast<float>(_region.x),
                                 static_cast<float>(_region.y),
                                 static_cast<float>(_region.width),
                                 static_cast<float>(_region.height),
                                 0.0f,
                                 1.0f};
    vkCmdSetViewportWithCountEXT(command_buffer, 1, &viewport);
    const VkRect2D scissor = {
        .offset = {_region.x, _region.y},
        .extent = {_region.width, _region.height},
    };

    vkCmdSetScissorWithCountEXT(command_buffer, 1, &scissor);
  }

  {
    SetVertexInput(command_buffer);
    auto shader_stages = std::array<VkShaderEXT, 2>{_vertex_shader.shader,
                                                    _fragment_shader.shader};
    auto shader_bits = std::array<VkShaderStageFlagBits, 2>{
        _vertex_shader.stage_flag, _fragment_shader.stage_flag};
    vkCmdBindShadersEXT(command_buffer,
                        static_cast<uint32_t>(shader_stages.size()),
                        shader_bits.data(), shader_stages.data());
    for (uint32_t i = 0; i < _render_layers.size(); ++i) {
      BindLayerDrawCommand(command_buffer, i);
      vkCmdDrawIndexed(command_buffer, _render_layers[i]->GetIndexCount(), 1, 0,
                       0, 0);
    }
  }
  {
    vkCmdEndRenderingKHR(command_buffer);
  }
}

void ModelRenderer::BindLayerDrawCommand(VkCommandBuffer command_buffer,
                                         uint32_t index) const {
  VkDescriptorImageInfo const image_info = {
      .sampler = _sampler,
      .imageView = _render_layers[index]->GetImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };

  VkWriteDescriptorSet const write_set = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = nullptr,
      .dstBinding = shader_gen::canvas_sd::main_tex.binding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = shader_gen::canvas_sd::main_tex.desc_type,
      .pImageInfo = &image_info,
  };

  VkDescriptorBufferInfo const buffer_info = {
      .buffer = _ubo_buffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE};
  VkWriteDescriptorSet const ubo_write_set = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = nullptr,
      .dstSet = nullptr,
      .dstBinding = shader_gen::canvas_sd::ubo.binding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = shader_gen::canvas_sd::ubo.desc_type,
      .pBufferInfo = &buffer_info,
  };

  auto write_sets =
      std::array<VkWriteDescriptorSet, 2>{ubo_write_set, write_set};
  vkCmdPushDescriptorSetKHR(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipeline_layout, 0, write_sets.size(),
                            write_sets.data());

  auto *vertex_buffer = _render_layers[index]->GetVertexBuffer();
  auto *index_buffer = _render_layers[index]->GetIndexBuffer();
  constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &offset);
  vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);
}
void ModelRenderer::AddLayer(Layer2dResource *layer) {
  _render_layers.push_back(layer);
}
ModelRenderer::~ModelRenderer() {
  auto *driver = VulkanDriver::GetSingleton();
  vkDeviceWaitIdle(driver->GetDevice());
  _vertex_shader.Destroy(driver->GetDevice());
  _fragment_shader.Destroy(driver->GetDevice());
  _ubo_buffer.Destroy(driver->GetVmaAllocator());
  vkDestroySampler(driver->GetDevice(), _sampler, nullptr);

  vkDestroyDescriptorSetLayout(driver->GetDevice(), _descriptor_set_layout,
                               nullptr);
  vkDestroyPipelineLayout(driver->GetDevice(), _pipeline_layout, nullptr);
}

void ModelRenderer::SetRegion(const int pos_x, const int pos_y,
                              const uint32_t width, const uint32_t height) {
  _region.x = pos_x;
  _region.y = pos_y;
  _region.width = width;
  _region.height = height;
  AutoCenterCanvas();
}
void ModelRenderer::SetCanvasSize(const uint32_t width, const uint32_t height) {
  _canvas_width = width;
  _canvas_height = height;
  AutoCenterCanvas();
}


ApplicationRenderer::ApplicationRenderer() {
  _model_renderer = std::make_unique<ModelRenderer>();
  auto driver = VulkanDriver::GetSingleton();
  assert(driver != nullptr && "VulkanDriver is not initialized");
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
    VkSemaphoreCreateInfo const semaphore_info = {
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
}
ApplicationRenderer::~ApplicationRenderer() {
  auto driver = VulkanDriver::GetSingleton();
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
  auto driver = VulkanDriver::GetSingleton();
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

  auto target_image = driver->GetSwapchainImages()[index];
  auto target_image_view = driver->GetSwapchainImageViews()[index];

  constexpr VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
  };
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