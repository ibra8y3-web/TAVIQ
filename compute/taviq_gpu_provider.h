#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TAVIQ_COMPUTE_ABI_VERSION 1u
typedef struct taviq_compute_device_v1 {
 const char* name;
 const char* backend;
 uint64_t memory_bytes;
} taviq_compute_device_v1;
typedef struct taviq_gpu_provider_v1 {
 uint32_t abi_version;
 const char* provider_name;
 int (*device_count)(void);
 int (*device_info)(int index,taviq_compute_device_v1* output);
 /* The provider owns device/context creation and must validate all lengths. */
 int (*vector_add_f32)(int device,const float* a,const float* b,float* output,size_t count);
 void (*shutdown)(void);
} taviq_gpu_provider_v1;
const taviq_gpu_provider_v1* taviq_compute_provider_entry_v1(void);
#ifdef __cplusplus
}
#endif
