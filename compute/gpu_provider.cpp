#include "taviq/gpu.hpp"
#include <utility>
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
constexpr std::size_t MaxVectorElements=16*1024*1024;
using EntryPoint=const taviq_gpu_provider_v1* (*)();
void closeLibrary(void* library) noexcept {
    if (!library) return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}
void* openLibrary(const std::string& path,std::string& error) {
#if defined(_WIN32)
    HMODULE handle=LoadLibraryA(path.c_str());
    if (!handle) { error="LoadLibrary failed with error "+std::to_string(GetLastError()); return nullptr; }
    return static_cast<void*>(handle);
#else
    dlerror();
    void* handle=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);
    if (!handle) { const char* detail=dlerror(); error=detail?detail:"dlopen failed"; return nullptr; }
    return handle;
#endif
}
EntryPoint findEntry(void* library,std::string& error) {
#if defined(_WIN32)
    FARPROC proc=GetProcAddress(static_cast<HMODULE>(library),"taviq_compute_provider_entry_v1");
    if (!proc) { error="GetProcAddress failed with error "+std::to_string(GetLastError()); return nullptr; }
    return reinterpret_cast<EntryPoint>(proc);
#else
    dlerror();
    void* address=dlsym(library,"taviq_compute_provider_entry_v1");
    const char* detail=dlerror();
    if (detail) { error=detail; return nullptr; }
    return reinterpret_cast<EntryPoint>(address);
#endif
}
}
GpuProvider::~GpuProvider() { unload(); }
void GpuProvider::unload() noexcept {
    if (api_ && api_->shutdown) {
        try { api_->shutdown(); } catch (...) { /* C ABI providers must not throw. */ }
    }
    api_=nullptr;
    providerName_.clear();
    closeLibrary(library_);
    library_=nullptr;
}
bool GpuProvider::load(const std::string& libraryPath,std::string& error) {
    unload();
    error.clear();
    if (libraryPath.empty()) { error="GPU provider library path is empty"; return false; }
    void* candidate=openLibrary(libraryPath,error);
    if (!candidate) return false;
    const EntryPoint entry=findEntry(candidate,error);
    if (!entry) { closeLibrary(candidate); return false; }
    const taviq_gpu_provider_v1* api=nullptr;
    try { api=entry(); }
    catch (...) { error="GPU provider entry point threw across the C ABI"; closeLibrary(candidate); return false; }
    if (!api) { error="GPU provider entry point returned null"; closeLibrary(candidate); return false; }
    if (api->abi_version!=TAVIQ_COMPUTE_ABI_VERSION) {
        error="GPU provider ABI version mismatch"; closeLibrary(candidate); return false;
    }
    if (!api->provider_name || api->provider_name[0]=='\0' ||
        !api->device_count || !api->device_info || !api->vector_add_f32) {
        error="GPU provider is missing a required ABI field"; closeLibrary(candidate); return false;
    }
    library_=candidate;
    api_=api;
    providerName_=api->provider_name;
    return true;
}
int GpuProvider::deviceCount(std::string& error) const {
    error.clear();
    if (!loaded()) { error="no GPU provider is loaded"; return -1; }
    int count=-1;
    try { count=api_->device_count(); }
    catch (...) { error="GPU provider device_count threw across the C ABI"; return -1; }
    if (count<0 || count>256) { error="GPU provider returned an invalid device count"; return -1; }
    return count;
}
bool GpuProvider::deviceInfo(int index,GpuDeviceInfo& output,std::string& error) const {
    error.clear();
    if (!loaded()) { error="no GPU provider is loaded"; return false; }
    const int count=deviceCount(error);
    if (count<0) return false;
    if (index<0 || index>=count) { error="GPU device index is out of range"; return false; }
    taviq_compute_device_v1 info{};
    int status=-1;
    try { status=api_->device_info(index,&info); }
    catch (...) { error="GPU provider device_info threw across the C ABI"; return false; }
    if (status!=0) { error="GPU provider failed to describe the requested device"; return false; }
    if (!info.name || !info.backend || info.name[0]=='\0' || info.backend[0]=='\0') {
        error="GPU provider returned incomplete device metadata"; return false;
    }
    output.name=info.name;
    output.backend=info.backend;
    output.memoryBytes=info.memory_bytes;
    return true;
}
bool GpuProvider::vectorAddF32(int device,const std::vector<float>& a,
                               const std::vector<float>& b,std::vector<float>& output,
                               std::string& error) const {
    error.clear();
    output.clear();
    if (!loaded()) { error="no GPU provider is loaded"; return false; }
    if (a.size()!=b.size()) { error="GPU vector-add inputs must have equal lengths"; return false; }
    if (a.size()>MaxVectorElements) { error="GPU vector-add input exceeds the 16M-element safety limit"; return false; }
    const int count=deviceCount(error);
    if (count<0) return false;
    if (device<0 || device>=count) { error="GPU device index is out of range"; return false; }
    output.resize(a.size());
    int status=-1;
    try { status=api_->vector_add_f32(device,a.data(),b.data(),output.data(),a.size()); }
    catch (...) { output.clear(); error="GPU provider vector_add_f32 threw across the C ABI"; return false; }
    if (status!=0) { output.clear(); error="GPU provider vector_add_f32 failed"; return false; }
    return true;
}
}
