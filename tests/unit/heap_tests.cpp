#include "taviq/heap.hpp"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"HEAP CHECK failed line "<<__LINE__<<": "#x"\n";return 1;}}while(false)
int main(){
 using namespace taviq::memory;Heap heap(1024*1024,100);
 const auto child=heap.makeString("reachable");
 const auto orphan=heap.makeString("orphan");
 const auto parent=heap.makeArray({child});
 auto root=heap.root(parent);
 CHECK(heap.liveObjects()==3);
 CHECK(heap.collect()==1);
 ObjectView view;CHECK(heap.inspect(child,view));CHECK(view.text=="reachable");
 CHECK(!heap.inspect(orphan,view));
 root.reset();CHECK(heap.collect()==2);CHECK(heap.liveObjects()==0);
 bool rejected=false;try{(void)heap.makeArray({orphan});}catch(const std::invalid_argument&){rejected=true;}
 CHECK(rejected);
 bool limit=false;try{Heap small(32,4);(void)small.makeString(std::string(100,'x'));}catch(const std::bad_alloc&){limit=true;}
 CHECK(limit);return 0;
}
