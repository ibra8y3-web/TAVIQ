#include "taviq/gpu.hpp"
#include <iostream>
#include <string>
#include <vector>
#ifndef TAVIQ_TEST_COMPUTE_PROVIDER_PATH
#error TAVIQ_TEST_COMPUTE_PROVIDER_PATH must point to the test fixture module
#endif
#define CHECK(c) do { if (!(c)) { std::cerr<<"CHECK failed at "<<__FILE__<<":"<<__LINE__<<": "#c"\n"; return 1; } } while(false)
int main() {
    taviq::GpuProvider provider;
    std::string error;
    CHECK(!provider.loaded());
    CHECK(provider.deviceCount(error)==-1);
    CHECK(error=="no GPU provider is loaded");
    CHECK(!provider.load("",error));
    CHECK(!provider.load("this-taviq-provider-does-not-exist",error));
    CHECK(!error.empty());
    CHECK(provider.load(TAVIQ_TEST_COMPUTE_PROVIDER_PATH,error));
    CHECK(provider.loaded());
    CHECK(provider.name()=="TAVIQ test-only provider");
    CHECK(provider.deviceCount(error)==1);
    taviq::GpuDeviceInfo info;
    CHECK(provider.deviceInfo(0,info,error));
    CHECK(info.name=="TAVIQ ABI fixture device");
    CHECK(info.backend=="fixture-only");
    CHECK(info.memoryBytes==1024*1024);
    CHECK(!provider.deviceInfo(1,info,error));
    CHECK(!provider.deviceInfo(-1,info,error));
    const std::vector<float> a={1.0f,2.0f,-4.0f};
    const std::vector<float> b={5.0f,3.0f,10.0f};
    std::vector<float> result;
    CHECK(provider.vectorAddF32(0,a,b,result,error));
    CHECK(result.size()==3);
    CHECK(result[0]==6.0f && result[1]==5.0f && result[2]==6.0f);
    CHECK(!provider.vectorAddF32(1,a,b,result,error));
    CHECK(result.empty());
    CHECK(!provider.vectorAddF32(0,a,std::vector<float>{1.0f},result,error));
    CHECK(result.empty());
    CHECK(provider.vectorAddF32(0,{}, {},result,error));
    CHECK(result.empty());
    provider.unload();
    CHECK(!provider.loaded());
    CHECK(provider.deviceCount(error)==-1);
    CHECK(provider.load(TAVIQ_TEST_COMPUTE_PROVIDER_PATH,error));
    provider.unload();
    return 0;
}
