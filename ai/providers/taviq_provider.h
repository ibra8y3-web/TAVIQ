#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define TAVIQ_AI_ABI_VERSION 1u
typedef struct taviq_ai_request_v1 { const char* prompt; size_t prompt_size; uint32_t max_tokens; double temperature; } taviq_ai_request_v1;
typedef struct taviq_ai_response_v1 { char* text; size_t text_size; char* error; } taviq_ai_response_v1;
typedef struct taviq_ai_provider_v1 {
 uint32_t abi_version;
 const char* provider_name;
 int (*initialize)(const char* configuration_json);
 int (*generate)(const taviq_ai_request_v1*,taviq_ai_response_v1*);
 void (*free_response)(taviq_ai_response_v1*);
 void (*shutdown)(void);
} taviq_ai_provider_v1;
/* A provider exports this symbol. TAVIQ checks ABI version before calling it. */
const taviq_ai_provider_v1* taviq_ai_provider_entry_v1(void);
#ifdef __cplusplus
}
#endif
