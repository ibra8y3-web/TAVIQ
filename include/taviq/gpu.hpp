#pragma once
#include "taviq/gpu_provider.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace taviq {
struct GpuDeviceInfo {
    std::string name;
    std::string backend;
    std::uint64_t memoryBytes{};
};
/* Explicitly loads one trusted provider library. One instance is not thread-safe. */
class GpuProvider {
public:
    GpuProvider() = default;
    ~GpuProvider();
    GpuProvider(const GpuProvider&) = delete;
    GpuProvider& operator=(const GpuProvider&) = delete;
    bool load(const std::string& libraryPath, std::string& error);
    void unload() noexcept;
    bool loaded() const noexcept { return library_ != nullptr && api_ != nullptr; }
    const std::string& name() const noexcept { return providerName_; }
    int deviceCount(std::string& error) const;
    bool deviceInfo(int index, GpuDeviceInfo& output, std::string& error) const;
    bool vectorAddF32(int device, const std::vector<float>& a,
                      const std::vector<float>& b, std::vector<float>& output,
                      std::string& error) const;
private:
    void* library_{nullptr};
    const taviq_gpu_provider_v1* api_{nullptr};
    std::string providerName_;
};
}
