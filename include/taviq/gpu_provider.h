#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TAVIQ_COMPUTE_ABI_VERSION 1u

#if defined(_WIN32) && defined(TAVIQ_COMPUTE_PROVIDER_BUILD)
#define TAVIQ_COMPUTE_EXPORT __declspec(dllexport)
#elif defined(_WIN32)
#define TAVIQ_COMPUTE_EXPORT __declspec(dllimport)
#else
#define TAVIQ_COMPUTE_EXPORT __attribute__((visibility("default")))
#endif

/* Provider functions return zero on success and non-zero on failure. */
typedef struct taviq_compute_device_v1 {
    const char* name;
    const char* backend;
    uint64_t memory_bytes;
} taviq_compute_device_v1;
typedef struct taviq_gpu_provider_v1 {
    uint32_t abi_version;
    const char* provider_name;
    int (*device_count)(void);
    int (*device_info)(int index, taviq_compute_device_v1* output);
    int (*vector_add_f32)(int device, const float* a, const float* b,
                          float* output, size_t count);
    void (*shutdown)(void); /* optional */
} taviq_gpu_provider_v1;

/* Providers define TAVIQ_COMPUTE_PROVIDER_BUILD while compiling their module. */
TAVIQ_COMPUTE_EXPORT const taviq_gpu_provider_v1* taviq_compute_provider_entry_v1(void);
#ifdef __cplusplus
}
#endif
