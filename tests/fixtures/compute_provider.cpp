#include "taviq/gpu_provider.h"
#include <cstddef>
namespace {
int deviceCount() { return 1; }
int deviceInfo(int index,taviq_compute_device_v1* output) {
    if (index!=0 || !output) return 1;
    output->name="TAVIQ ABI fixture device";
    output->backend="fixture-only";
    output->memory_bytes=1024*1024;
    return 0;
}
int vectorAdd(int device,const float* a,const float* b,float* output,std::size_t count) {
    if (device!=0 || (count && (!a || !b || !output))) return 1;
    for (std::size_t i=0;i<count;++i) output[i]=a[i]+b[i];
    return 0;
}
void shutdown() {}
const taviq_gpu_provider_v1 provider={
    TAVIQ_COMPUTE_ABI_VERSION,"TAVIQ test-only provider",
    &deviceCount,&deviceInfo,&vectorAdd,&shutdown
};
}
extern "C" const taviq_gpu_provider_v1* taviq_compute_provider_entry_v1(void) { return &provider; }
