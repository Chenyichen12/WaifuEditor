#include "vulkan_driver.h"

#include <cstdint>
#include <iostream>
#include <set>
#include <cstring>
#include "vulkan/vulkan_core.h"

namespace {

void AddContainer(std::vector<const char *> &container, const char *item) {
  for (const auto &existing_item : container) {
    if (strcmp(existing_item, item) == 0) {
      return;  // Item already exists, do not add again
    }
  }
  container.push_back(item);
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void * /*pUserData*/) {
  if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ||
      messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::cerr << "validation layer: " << pCallbackData->pMessage << '\n';
  }
  return VK_FALSE;
};

}  // namespace

namespace rdc {
void AssertVkResult(const VkResult &result) {
  if (result != VK_SUCCESS) {
    // throw std::runtime_error("vulkan assert failed");
    std::cerr << "vulkan assert failed" << "\n";
    std::abort();
  }
}
void AssertVkResult(const VkResult &result, const char *message) {
  if (result != VK_SUCCESS) {
    // throw std::runtime_error(message);
    std::cerr << message << "\n";
    std::abort();
  }
}
VulkanDriver::VulkanDriver(const VulkanDriverConfig &config) {
  AssertVkResult(volkInitialize());

  {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "waifu_studio_rdc",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "waifu",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1,
    };

    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext = nullptr,
        .flags = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
        .pUserData = nullptr,
    };

    auto exts = config.instance_extensions;
    AddContainer(exts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = &debug_info,
        .pApplicationInfo = &app_info,
        .enabledLayerCount =
            static_cast<uint32_t>(config.instance_layers.size()),
        .ppEnabledLayerNames = config.instance_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(exts.size()),
        .ppEnabledExtensionNames = exts.data(),
    };

    AssertVkResult(vkCreateInstance(&instance_info, nullptr, &_instance),
                   "Failed to create Vulkan instance");
    // create debug messenger

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func == nullptr) {
      std::cerr << "Failed to load vkCreateDebugUtilsMessengerEXT function\n";
      abort();
    }
    AssertVkResult(func(_instance, &debug_info, nullptr, &_debug_messenger),
                   "Failed to create debug messenger");
    volkLoadInstance(_instance);
  }
  {
    AssertVkResult(config.create_surface_callback(_instance, _surface),
                   "Failed to create Vulkan surface");
  }
  {
    // pick one physical device
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    AssertVkResult(vkEnumeratePhysicalDevices(_instance, &device_count,
                                              physical_devices.data()),
                   "Failed to enumerate physical devices");
    VkPhysicalDevice selected_device = VK_NULL_HANDLE;
    // select logic
    for (const auto &device : physical_devices) {
      VkPhysicalDeviceProperties device_properties;
      vkGetPhysicalDeviceProperties(device, &device_properties);
      if (device_properties.deviceType ==
          VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        selected_device = device;
        std::cout << "Selected physical device: "
                  << device_properties.deviceName << "\n";
        break;
      }
    }
    if (selected_device == VK_NULL_HANDLE) {
      std::cerr << "No suitable physical device found\n";
      std::abort();
    }

    // select queue
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(selected_device,
                                             &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        selected_device, &queue_family_count, queue_families.data());
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;

    for (uint32_t i = 0; i < queue_family_count; ++i) {
      if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphics_queue_family_index = i;
        // break;
      }
      VkBool32 present_support = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(selected_device, i, _surface,
                                           &present_support);
      if (present_support) {
        present_queue_family_index = i;
      }
      if (graphics_queue_family_index != UINT32_MAX &&
          present_queue_family_index != UINT32_MAX) {
        break;  // Found both graphics and present queue families
      }
    }

    if (graphics_queue_family_index == UINT32_MAX ||
        present_queue_family_index == UINT32_MAX) {
      std::cerr << "No suitable graphics queue family found\n";
      std::abort();
    }

    // create device
    std::set<uint32_t> unique_queue_families = {
        graphics_queue_family_index,
        present_queue_family_index,
    };

    std::vector<VkDeviceQueueCreateInfo> queue_infos;
    float queue_priority = 1.0f;
    for (const auto &family_index : unique_queue_families) {
      VkDeviceQueueCreateInfo queue_info = {
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0,
          .queueFamilyIndex = family_index,
          .queueCount = 1,
          .pQueuePriorities = &queue_priority,
      };
      queue_infos.push_back(queue_info);
    }

    // create logical device
    std::vector<const char *> device_extensions = config.device_extensions;
    AddContainer(device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    AddContainer(device_extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    AddContainer(device_extensions, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    AddContainer(device_extensions, VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
    AddContainer(device_extensions,
                 VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
    AddContainer(device_extensions, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    AddContainer(device_extensions,
                 VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT dynamic_state_ext = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
        .extendedDynamicState = VK_TRUE,
    };
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &dynamic_state_ext,
        .dynamicRendering = VK_TRUE,

    };

    VkPhysicalDeviceShaderObjectFeaturesEXT shader_object_features = {};
    shader_object_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT;
    shader_object_features.shaderObject = VK_TRUE;
    shader_object_features.pNext = &dynamic_rendering_features;

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        // .pNext = &dynamic_rendering_features,
        .pNext = &shader_object_features,
        .queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size()),
        .pQueueCreateInfos = queue_infos.data(),
        .enabledExtensionCount =
            static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
    };

    AssertVkResult(
        vkCreateDevice(selected_device, &device_info, nullptr, &_device),
        "Failed to create Vulkan device");
    volkLoadDevice(_device);
    _queue_packet.graphics_queue_family_index = graphics_queue_family_index;
    vkGetDeviceQueue(_device, graphics_queue_family_index, 0,
                     &_queue_packet.graphics_queue);
    vkGetDeviceQueue(_device, present_queue_family_index, 0,
                     &_queue_packet.present_queue);
    _queue_packet.present_queue_family_index = present_queue_family_index;
    _queue_packet.graphics_queue_family_index = graphics_queue_family_index;
    std::cout << "Created Vulkan device with graphics queue family index: "
              << graphics_queue_family_index
              << ", present queue family index: " << present_queue_family_index
              << "\n";

    // swapchain details
    _swapchain_packet.QuerySwapchainSupport(selected_device, _surface);
    {
      VmaVulkanFunctions vulkan_functions = {};
      vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
      vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
      // vma
      VmaAllocatorCreateInfo vma_allocator_info = {
          .physicalDevice = selected_device,
          .device = _device,
          .instance = _instance,
          .vulkanApiVersion = VK_API_VERSION_1_1,
      };
      vma_allocator_info.pVulkanFunctions = &vulkan_functions;

      AssertVkResult(vmaCreateAllocator(&vma_allocator_info, &_vma_allocator),
                     "Failed to create VMA allocator");
    }
  }
  {
    CreateSwapchain({config.initial_width, config.initial_height});
  }

  // create command pool
  {
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = _queue_packet.graphics_queue_family_index,
    };
    AssertVkResult(vkCreateCommandPool(_device, &command_pool_info, nullptr,
                                       &_command_pool),
                   "Failed to create command pool");
  }
}
void VulkanDriver::CreateSwapchain(const VkExtent2D &extent) {
  auto surface_format = _swapchain_packet.ChooseSurfaceFormat();
  auto present_mode = _swapchain_packet.ChoosePresentMode();
  auto swapchain_extent =
      _swapchain_packet.ChooseExtent(extent.width, extent.height);
  auto image_count = _swapchain_packet.capabilities.minImageCount + 1;
  auto *old_swapchain = _swapchain_packet.swapchain;

  VkSwapchainCreateInfoKHR swapchain_info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = _surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform = _swapchain_packet.capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = old_swapchain,
  };
  uint32_t queue_family_indices[] = {
      _queue_packet.graphics_queue_family_index,
      _queue_packet.present_queue_family_index,
  };
  if (_queue_packet.graphics_queue_family_index !=
      _queue_packet.present_queue_family_index) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = 2;
    swapchain_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  AssertVkResult(vkCreateSwapchainKHR(_device, &swapchain_info, nullptr,
                                      &_swapchain_packet.swapchain),
                 "Failed to create swapchain");
  // destroy old swapchain
  vkDestroySwapchainKHR(_device, old_swapchain, nullptr);
  // destroy old image views
  for (auto &image_view : _swapchain_packet.image_views) {
    vkDestroyImageView(_device, image_view, nullptr);
  }

  // get infomations
  _swapchain_packet.extent = swapchain_extent;
  _swapchain_packet.image_format = surface_format.format;

  vkGetSwapchainImagesKHR(_device, _swapchain_packet.swapchain, &image_count,
                          nullptr);
  _swapchain_packet.images.resize(image_count);
  _swapchain_packet.image_views.resize(image_count);
  AssertVkResult(
      vkGetSwapchainImagesKHR(_device, _swapchain_packet.swapchain,
                              &image_count, _swapchain_packet.images.data()),
      "Failed to get swapchain images");

  for (size_t i = 0; i < _swapchain_packet.images.size(); ++i) {
    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = _swapchain_packet.images[i],
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surface_format.format,
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
    AssertVkResult(vkCreateImageView(_device, &image_view_info, nullptr,
                                     &_swapchain_packet.image_views[i]),
                   "Failed to create image view for swapchain image");
  }
}

VkSampler VulkanDriver::HCreateSimpleSampler() const {
  VkSamplerCreateInfo sampler_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.0f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.0f,
      .maxLod = VK_LOD_CLAMP_NONE,
      .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
  };
  VkSampler sampler;
  AssertVkResult(vkCreateSampler(_device, &sampler_info, nullptr, &sampler),
                 "Failed to create sampler");
  return sampler;
}
VkCommandBuffer VulkanDriver::HBeginOneTimeCommandBuffer() const {
  VkCommandBufferAllocateInfo alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = _command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };
  VkCommandBuffer command_buffer;
  AssertVkResult(
      vkAllocateCommandBuffers(_device, &alloc_info, &command_buffer),
      "Failed to allocate command buffer");
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr,
  };
  AssertVkResult(vkBeginCommandBuffer(command_buffer, &begin_info),
                 "Failed to begin command buffer");
  return command_buffer;
}
void VulkanDriver::HEndOneTimeCommandBuffer(
    const VkCommandBuffer &command_buffer, const VkQueue &submit_queue) const {
  // vkEndCommandBuffer(command_buffer);
  AssertVkResult(vkEndCommandBuffer(command_buffer));
  const VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };
  vkQueueSubmit(submit_queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(submit_queue);
  vkFreeCommandBuffers(_device, _command_pool, 1, &command_buffer);
}

void VulkanDriver::HTransitionImageLayout(
    const VkCommandBuffer &cmd, const VkImage &image, const VkAccessFlags src,
    const VkAccessFlags dst, const VkPipelineStageFlags src_stage,
    const VkPipelineStageFlags dst_stage, const VkImageLayout old_layout,
    const VkImageLayout new_layout,
    const VkImageSubresourceRange &range) const {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = src,
      .dstAccessMask = dst,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = _queue_packet.graphics_queue_family_index,
      .dstQueueFamilyIndex = _queue_packet.graphics_queue_family_index,
      .image = image,
      .subresourceRange = range,
  };
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

void VulkanDriver::HCreateBuffer(const uint32_t size,
                                 const VkBufferUsageFlags usage,
                                 const VmaMemoryUsage vma_flags,
                                 VkBuffer &buffer,
                                 VmaAllocation &allocation) const {
  VmaAllocationCreateInfo alloc_info = {
      .usage = vma_flags,
  };
  VkBufferCreateInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  AssertVkResult(vmaCreateBuffer(_vma_allocator, &buffer_info, &alloc_info,
                                 &buffer, &allocation, nullptr),
                 "Failed to create buffer");
}

VulkanDriver::~VulkanDriver() {
  if (_debug_messenger != VK_NULL_HANDLE) {
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
    func(_instance, _debug_messenger, nullptr);
  }
  vmaDestroyAllocator(_vma_allocator);
  vkDestroyCommandPool(_device, _command_pool, nullptr);
  _swapchain_packet.Destroy(_device);
  vkDestroySurfaceKHR(_instance, _surface, nullptr);
  vkDestroyDevice(_device, nullptr);
  vkDestroyInstance(_instance, nullptr);
}

void VulkanDriver::SwapchainPacket::QuerySwapchainSupport(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                       nullptr);
  if (format_count > 0) {
    formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface,
                                         &format_count, formats.data());
  }
  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, nullptr);
  if (present_mode_count > 0) {
    present_modes.resize(present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, present_modes.data());
  }
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                            &capabilities);
}

VkSurfaceFormatKHR VulkanDriver::SwapchainPacket::ChooseSurfaceFormat() const {
  for (const auto &format : formats) {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats[0];  // Fallback to the first format
}

VkPresentModeKHR VulkanDriver::SwapchainPacket::ChoosePresentMode() const {
  for (const auto &mode : present_modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;  // Fallback to the default mode
}
VkExtent2D VulkanDriver::SwapchainPacket::ChooseExtent(uint32_t width,
                                                       uint32_t height) const {
  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;  // Use the current extent
  }
  return {width, height};
}

void VulkanDriver::SwapchainPacket::Destroy(VkDevice device) {
  for (auto &image_view : image_views) {
    vkDestroyImageView(device, image_view, nullptr);
  }
  vkDestroySwapchainKHR(device, swapchain, nullptr);
}
VulkanDriver *VulkanDriver::singleton_driver = nullptr;
}  // namespace rdc