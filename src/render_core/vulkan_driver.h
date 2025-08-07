#ifndef SRC_RENDER_CORE_VULKAN_DRIVER_H_
#define SRC_RENDER_CORE_VULKAN_DRIVER_H_

#include <vk_mem_alloc.h>
#include <volk.h>

#include <cstdint>
#include <functional>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace rdc {

void AssertVkResult(const VkResult &result);
void AssertVkResult(const VkResult &result, const char *message);
struct VulkanDriverConfig {
  std::vector<const char *> instance_extensions;
  std::vector<const char *> instance_layers;
  std::vector<const char *> device_extensions;
  uint32_t initial_height;
  uint32_t initial_width;
  std::function<VkResult(VkInstance instance, VkSurfaceKHR &surface)>
      create_surface_callback;
};

class VulkanDriver {
  VkInstance _instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT _debug_messenger = VK_NULL_HANDLE;

  struct QueuePacket {
    uint32_t graphics_queue_family_index = UINT32_MAX;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    uint32_t present_queue_family_index = UINT32_MAX;
    VkQueue present_queue = VK_NULL_HANDLE;
  } _queue_packet;
  VkDevice _device = VK_NULL_HANDLE;
  VkSurfaceKHR _surface = VK_NULL_HANDLE;
  VmaAllocator _vma_allocator = VK_NULL_HANDLE;
  class SwapchainPacket {
   public:
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat image_format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0, 0};
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    uint32_t current_image_index = 0;

    VkPresentModeKHR ChoosePresentMode() const;
    VkSurfaceFormatKHR ChooseSurfaceFormat() const;
    VkExtent2D ChooseExtent(uint32_t width, uint32_t height) const;
    void QuerySwapchainSupport(VkPhysicalDevice physical_device,
                               VkSurfaceKHR surface);
    void Destroy(VkDevice device);

    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
    VkSurfaceCapabilitiesKHR capabilities;

  } _swapchain_packet;
  VkCommandPool _command_pool = VK_NULL_HANDLE;

  void CreateSwapchain(const VkExtent2D &extent);

 public:
  explicit VulkanDriver(const VulkanDriverConfig &config);

  // getter for vulkan driver

  /// swapchains
  const VkSwapchainKHR &GetSwapchain() const {
    return _swapchain_packet.swapchain;
  }
  const VkFormat &GetSwapchainFormat() const {
    return _swapchain_packet.image_format;
  }
  const std::vector<VkImageView> &GetSwapchainImageViews() const {
    return _swapchain_packet.image_views;
  }
  const std::vector<VkImage> &GetSwapchainImages() const {
    return _swapchain_packet.images;
  }
  const VkExtent2D &GetSwapchainExtent() const {
    return _swapchain_packet.extent;
  }

  uint32_t GetCurrentSwapchainImageIndex() const {
    return _swapchain_packet.current_image_index;
  }
  int AcquireNextSwapchainImage(const VkSemaphore &semaphore,
                                const VkFence &fence) {
    vkAcquireNextImageKHR(_device, _swapchain_packet.swapchain, UINT64_MAX,
                          semaphore, fence,
                          &_swapchain_packet.current_image_index);
    return static_cast<int>(_swapchain_packet.current_image_index);
  }

  const VkQueue &GetGraphicsQueue() const {
    return _queue_packet.graphics_queue;
  }
  const VkQueue &GetPresentQueue() const { return _queue_packet.present_queue; }
  const uint32_t &GetGraphicsQueueFamilyIndex() const {
    return _queue_packet.graphics_queue_family_index;
  }
  const uint32_t &GetPresentQueueFamilyIndex() const {
    return _queue_packet.present_queue_family_index;
  }

  const VkDevice &GetDevice() const { return _device; }
  const VkCommandPool &GetCommandPool() const { return _command_pool; }
  const VmaAllocator &GetVmaAllocator() const { return _vma_allocator; }

  // helpers
  VkSampler HCreateSimpleSampler() const;
  VkCommandBuffer HBeginOneTimeCommandBuffer() const;
  void HEndOneTimeCommandBuffer(const VkCommandBuffer &command_buffer,
                                const VkQueue &submit_queue) const;
  VkCommandBuffer HCreateOneCommandBuffer() const;

  enum class TransitionState : uint8_t {
    kInit2Transfer,
    kTransfer2Read,
  };
  void HTransitionImageLayout(const VkCommandBuffer &cmd, const VkImage &image,
                              const TransitionState &state,
                              const VkImageSubresourceRange &range = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1,
                              });

  void HTransitionImageLayout(const VkCommandBuffer &cmd, const VkImage &image,
                              VkAccessFlags src_access,
                              VkAccessFlags dst_access,
                              VkPipelineStageFlags src_stage,
                              VkPipelineStageFlags dst_stage,
                              VkImageLayout old_layout,
                              VkImageLayout new_layout,
                              const VkImageSubresourceRange &range = {
                                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1,
                              }) const;
  void HCreateBuffer(uint32_t size, VkBufferUsageFlags usage,
                     VmaMemoryUsage vma_flags, VkBuffer &buffer,
                     VmaAllocation &allocation) const;

  ~VulkanDriver();

  // global vulkan
 private:
  static VulkanDriver *singleton_driver;

 public:
  static void InitSingleton(const VulkanDriverConfig &config) {
    singleton_driver = new VulkanDriver(config);
  }
  static VulkanDriver *GetSingleton() { return singleton_driver; }
  static void CleanupSingleton() {
    delete singleton_driver;
    singleton_driver = nullptr;
  }
};

}  // namespace rdc

#endif  // SRC_RENDER_CORE_VULKAN_DRIVER_H_
