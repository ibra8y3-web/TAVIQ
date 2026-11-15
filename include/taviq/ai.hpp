#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include "taviq/ai_provider.h"
namespace taviq {
struct AiResult { std::string text; };
class AiProvider {
public:
    AiProvider() = default;
    ~AiProvider();
    AiProvider(const AiProvider&) = delete;
    AiProvider& operator=(const AiProvider&) = delete;
    bool load(const std::string& libraryPath,const std::string& configurationJson,
              std::string& error);
    bool generate(const std::string& prompt,std::uint32_t maxTokens,double temperature,
                  AiResult& result,std::string& error) const;
    void unload() noexcept;
    bool loaded() const noexcept { return library_ && api_; }
    const std::string& name() const noexcept { return providerName_; }
private:
    void* library_{nullptr};
    const ::taviq_ai_provider_v1* api_{nullptr};
    std::string providerName_;
};
}
