#include "taviq/compute.hpp"
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
namespace taviq {
void parallelFor(std::size_t begin,std::size_t end,std::size_t grain,const std::function<void(std::size_t)>& fn,std::size_t workers){
 if(end<=begin)return;if(grain==0)grain=1;
 const auto count=end-begin;const auto chunks=(count+grain-1)/grain;
 if(workers==0)workers=std::thread::hardware_concurrency();workers=std::max<std::size_t>(1,std::min(workers,chunks));
 std::atomic<std::size_t> next{begin};std::atomic<bool> stop{false};std::exception_ptr failure;std::mutex failureMutex;std::vector<std::thread> pool;
 auto run=[&]{try{while(!stop.load(std::memory_order_relaxed)){auto start=next.fetch_add(grain);if(start>=end)break;auto limit=std::min(end,start+grain);for(auto i=start;i<limit;++i){if(stop.load(std::memory_order_relaxed))break;fn(i);}}}catch(...){stop.store(true);std::lock_guard<std::mutex> lock(failureMutex);if(!failure)failure=std::current_exception();}};
 for(std::size_t n=0;n<workers;++n)pool.emplace_back(run);for(auto& t:pool)t.join();if(failure)std::rethrow_exception(failure);
}
}