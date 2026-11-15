#include "taviq/ai.hpp"
#include <iostream>
#include <string>
#ifndef TAVIQ_TEST_AI_PROVIDER_PATH
#error missing AI test provider path
#endif
#define CHECK(x) do{if(!(x)){std::cerr<<"CHECK failed line "<<__LINE__<<": "#x"\n";return 1;}}while(false)
int main(){
 taviq::AiProvider provider;std::string error;taviq::AiResult result;
 CHECK(!provider.generate("x",1,0.5,result,error));
 CHECK(error=="no AI provider is loaded");
 CHECK(!provider.load("", "{}", error));
 CHECK(!provider.load("missing-taviq-ai-provider", "{}", error));
 CHECK(provider.load(TAVIQ_TEST_AI_PROVIDER_PATH,"{}",error));
 CHECK(provider.name()=="TAVIQ contract-test provider");
 CHECK(provider.generate("hello",32,0.2,result,error));
 CHECK(result.text=="fixture:hello");
 CHECK(!provider.generate("",32,0.2,result,error));
 CHECK(!provider.generate("x",0,0.2,result,error));
 CHECK(!provider.generate("x",32,3.0,result,error));
 CHECK(!provider.generate(std::string(1024*1024+1,'x'),32,0.2,result,error));
 provider.unload();
 CHECK(!provider.loaded());
 return 0;
}
