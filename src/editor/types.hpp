//
// Created by chenyichen on 25-7-27.
//

#ifndef EDITOR_TYPES_HPP
#define EDITOR_TYPES_HPP
#include <stb_image.h>
#include <stb_image_write.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

class CPUImage {
 public:
  uint32_t width = 0;
  uint32_t height = 0;
  int channels = 0;
  void* data = nullptr;
  std::function<void()> deleter = nullptr;

  CPUImage() = default;
  CPUImage(const CPUImage& img) {
    width = img.width;
    height = img.height;
    channels = img.channels;
    data = new uint8_t[static_cast<uint64_t>(width * height * channels)];
    memcpy(data, img.data, width * height * channels);
    deleter = [this]() {
      free(data);
      data = nullptr;
    };
  }
  CPUImage(CPUImage&& img) noexcept
      : width(img.width),
        height(img.height),
        channels(img.channels),
        data(img.data) {
    deleter = img.deleter;
    img.data = nullptr;
    img.deleter = nullptr;
  }

  bool IsValid() const { return data != nullptr; }

  void LoadFromFile(const std::string& file_path) {
    data = stbi_load(file_path.c_str(), reinterpret_cast<int*>(&width),
                     reinterpret_cast<int*>(&height), &channels, 4);
    if (data == nullptr) {
      return;
    }
    deleter = [this]() {
      if (data) {
        stbi_image_free(data);
        data = nullptr;
      }
    };
  }

  void SaveToFile(const std::string& path) const {
    stbi_write_png(path.c_str(), width, height, channels, data, 0);
  }

  ~CPUImage() {
    if (deleter) {
      deleter();
    }
  }
};

#endif  // EDITOR_TYPES_HPP
