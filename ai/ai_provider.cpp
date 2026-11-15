#include "taviq/ai.hpp"
#include "taviq/ai_provider.h"
#include <cmath>
#include <cstring>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace taviq {
namespace {
constexpr std::size_t MaxPromptBytes=1024*1024;
constexpr std::size_t MaxResponseBytes=8*1024*1024;
using EntryPoint=const taviq_ai_provider_v1* (*)();
void closeLibrary(void* handle) noexcept {
    if(!handle)return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}
void* openLibrary(const std::string& path,std::string& error) {
#if defined(_WIN32)
    HMODULE handle=LoadLibraryA(path.c_str());
    if(!handle)error="LoadLibrary failed with error "+std::to_string(GetLastError());
    return static_cast<void*>(handle);
#else
    dlerror();
    void* handle=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!handle){const char* e=dlerror();error=e?e:"dlopen failed";}
    return handle;
#endif
}
EntryPoint findEntry(void* handle,std::string& error) {
#if defined(_WIN32)
    FARPROC p=GetProcAddress(static_cast<HMODULE>(handle),"taviq_ai_provider_entry_v1");
    if(!p){error="GetProcAddress failed with error "+std::to_string(GetLastError());return nullptr;}
    return reinterpret_cast<EntryPoint>(p);
#else
    dlerror();
    void* p=dlsym(handle,"taviq_ai_provider_entry_v1");
    const char* e=dlerror();
    if(e){error=e;return nullptr;}
    return reinterpret_cast<EntryPoint>(p);
#endif
}
}
AiProvider::~AiProvider(){unload();}
void AiProvider::unload() noexcept {
    if(api_&&api_->shutdown){try{api_->shutdown();}catch(...){}}
    api_=nullptr;
    providerName_.clear();
    closeLibrary(library_);
    library_=nullptr;
}
bool AiProvider::load(const std::string& path,const std::string& config,std::string& error) {
    unload();
    error.clear();
    if(path.empty()){error="AI provider library path is empty";return false;}
    if(config.size()>64*1024){error="AI provider configuration exceeds 64 KiB";return false;}
    void* candidate=openLibrary(path,error);
    if(!candidate)return false;
    const auto entry=findEntry(candidate,error);
    if(!entry){closeLibrary(candidate);return false;}
    const taviq_ai_provider_v1* api=nullptr;
    try{api=entry();}catch(...){error="AI provider entry point threw across C ABI";closeLibrary(candidate);return false;}
    if(!api){error="AI provider entry point returned null";closeLibrary(candidate);return false;}
    if(api->abi_version!=TAVIQ_AI_ABI_VERSION){error="AI provider ABI version mismatch";closeLibrary(candidate);return false;}
    if(!api->provider_name||!api->provider_name[0]||!api->initialize||
       !api->generate||!api->free_response||!api->shutdown){
        error="AI provider is missing a required ABI callback";closeLibrary(candidate);return false;
    }
    int status=-1;
    try{status=api->initialize(config.c_str());}
    catch(...){error="AI provider initialize threw across C ABI";closeLibrary(candidate);return false;}
    if(status!=0){error="AI provider initialization failed";closeLibrary(candidate);return false;}
    library_=candidate;
    api_=api;
    providerName_=api->provider_name;
    return true;
}
bool AiProvider::generate(const std::string& prompt,std::uint32_t maxTokens,double temperature,
                          AiResult& result,std::string& error) const {
    error.clear();
    result.text.clear();
    if(!loaded()){error="no AI provider is loaded";return false;}
    if(prompt.empty()||prompt.size()>MaxPromptBytes){error="prompt must contain 1 byte to 1 MiB";return false;}
    if(maxTokens==0||maxTokens>8192){error="max_tokens must be between 1 and 8192";return false;}
    if(!std::isfinite(temperature)||temperature<0.0||temperature>2.0){
        error="temperature must be finite and between 0 and 2";return false;
    }
    const taviq_ai_request_v1 request{prompt.data(),prompt.size(),maxTokens,temperature};
    taviq_ai_response_v1 response{};
    int status=-1;
    try{status=api_->generate(&request,&response);}
    catch(...){error="AI provider generate threw across C ABI";return false;}
    const auto release=[&](){
        try{api_->free_response(&response);}catch(...){}
    };
    if(status!=0){
        if(response.error)error=std::string(response.error).substr(0,4096);
        if(error.empty())error="AI provider generation failed";
        release();
        return false;
    }
    if(!response.text||response.text_size==0||response.text_size>MaxResponseBytes){
        error="AI provider returned an empty or oversized response";
        release();
        return false;
    }
    result.text.assign(response.text,response.text_size);
    release();
    return true;
}
}
