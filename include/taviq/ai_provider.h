#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TAVIQ_AI_ABI_VERSION 1u
#if defined(_WIN32) && defined(TAVIQ_AI_PROVIDER_BUILD)
#define TAVIQ_AI_EXPORT __declspec(dllexport)
#elif defined(_WIN32)
#define TAVIQ_AI_EXPORT __declspec(dllimport)
#else
#define TAVIQ_AI_EXPORT __attribute__((visibility("default")))
#endif
typedef struct taviq_ai_request_v1 {
    const char* prompt;
    size_t prompt_size;
    uint32_t max_tokens;
    double temperature;
} taviq_ai_request_v1;
typedef struct taviq_ai_response_v1 {
    char* text;
    size_t text_size;
    char* error;
} taviq_ai_response_v1;
typedef struct taviq_ai_provider_v1 {
    uint32_t abi_version;
    const char* provider_name;
    int (*initialize)(const char* configuration_json);
    int (*generate)(const taviq_ai_request_v1*,taviq_ai_response_v1*);
    void (*free_response)(taviq_ai_response_v1*);
    void (*shutdown)(void);
} taviq_ai_provider_v1;
TAVIQ_AI_EXPORT const taviq_ai_provider_v1* taviq_ai_provider_entry_v1(void);
#ifdef __cplusplus
}
#endif
