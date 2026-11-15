#include "taviq/ai_provider.h"
#include <cstdlib>
#include <cstring>
#include <string>
namespace {
int initialize(const char*){return 0;}
int generate(const taviq_ai_request_v1* request,taviq_ai_response_v1* response){
 if(!request||!response||!request->prompt||request->prompt_size==0)return 1;
 const std::string value="fixture:"+std::string(request->prompt,request->prompt_size);
 response->text=static_cast<char*>(std::malloc(value.size()));
 if(!response->text)return 2;
 std::memcpy(response->text,value.data(),value.size());
 response->text_size=value.size();
 response->error=nullptr;
 return 0;
}
void freeResponse(taviq_ai_response_v1* response){
 if(!response)return;
 std::free(response->text);
 std::free(response->error);
 response->text=nullptr;response->text_size=0;response->error=nullptr;
}
void shutdown(){}
const taviq_ai_provider_v1 provider={TAVIQ_AI_ABI_VERSION,"TAVIQ contract-test provider",
 initialize,generate,freeResponse,shutdown};
}
extern "C" TAVIQ_AI_EXPORT const taviq_ai_provider_v1* taviq_ai_provider_entry_v1(void){return &provider;}
