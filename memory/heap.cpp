#include "taviq/heap.hpp"
#include <stdexcept>
#include <utility>
namespace taviq::memory {
Heap::Heap(std::size_t byteLimit,std::size_t objectLimit):byteLimit_(byteLimit),objectLimit_(objectLimit){}
Heap::Root::~Root(){reset();}
Heap::Root::Root(Root&& other)noexcept:heap_(other.heap_),id_(other.id_){other.heap_=nullptr;other.id_=0;}
Heap::Root& Heap::Root::operator=(Root&& other)noexcept{
 if(this!=&other){reset();heap_=other.heap_;id_=other.id_;other.heap_=nullptr;other.id_=0;}return *this;
}
void Heap::Root::reset()noexcept{if(heap_&&id_)heap_->unroot(id_);heap_=nullptr;id_=0;}
ObjectId Heap::allocate(Object object){
 std::lock_guard lock(mutex_);
 if(objects_.size()>=objectLimit_)throw std::bad_alloc();
 if(object.bytes>byteLimit_-bytesUsed_)throw std::bad_alloc();
 const ObjectId id=static_cast<ObjectId>(objects_.size())+1;
 for(const auto child:object.references)if(child==0||child>objects_.size()||objects_[child-1].bytes==0)throw std::invalid_argument("heap reference is not live");
 for(const auto& field:object.fields)if(field.first.empty()||field.second==0||field.second>objects_.size()||objects_[field.second-1].bytes==0)throw std::invalid_argument("record field contains an invalid reference");
 bytesUsed_+=object.bytes;objects_.push_back(std::move(object));return id;
}
ObjectId Heap::makeString(std::string value){
 Object object;object.kind=Kind::String;object.bytes=value.size()+sizeof(Object);object.text=std::move(value);return allocate(std::move(object));
}
ObjectId Heap::makeArray(std::vector<ObjectId> values){
 Object object;object.kind=Kind::Array;object.bytes=sizeof(Object)+values.size()*sizeof(ObjectId);object.references=std::move(values);return allocate(std::move(object));
}
ObjectId Heap::makeRecord(std::map<std::string,ObjectId> values){
 Object object;object.kind=Kind::Record;object.bytes=sizeof(Object);
 for(const auto& field:values)object.bytes+=field.first.size()+sizeof(ObjectId);
 object.fields=std::move(values);return allocate(std::move(object));
}
Heap::Root Heap::root(ObjectId id){
 std::lock_guard lock(mutex_);
 if(id==0||id>objects_.size()||objects_[id-1].bytes==0)throw std::invalid_argument("cannot root a stale heap object");
 ++roots_[id];return Root(this,id);
}
void Heap::unroot(ObjectId id)noexcept{
 std::lock_guard lock(mutex_);auto it=roots_.find(id);
 if(it!=roots_.end()){if(it->second>1)--it->second;else roots_.erase(it);}
}
bool Heap::inspect(ObjectId id,ObjectView& out)const{
 std::lock_guard lock(mutex_);if(id==0||id>objects_.size()||objects_[id-1].bytes==0)return false;
 const auto& obj=objects_[id-1];out={obj.kind,obj.text,obj.references,obj.fields};return true;
}
std::size_t Heap::collect(){
 std::lock_guard lock(mutex_);
 std::vector<ObjectId> work;work.reserve(roots_.size());
 for(const auto& root:roots_)work.push_back(root.first);
 while(!work.empty()){
  const auto id=work.back();work.pop_back();
  if(id==0||id>objects_.size())continue;
  auto& obj=objects_[id-1];if(obj.bytes==0||obj.marked)continue;
  obj.marked=true;
  for(auto child:obj.references)work.push_back(child);
  for(const auto& field:obj.fields)work.push_back(field.second);
 }
 std::size_t collected=0;
 for(auto& obj:objects_){
  if(obj.bytes==0)continue;
  if(obj.marked)obj.marked=false;
  else{bytesUsed_-=obj.bytes;obj.bytes=0;obj.text.clear();obj.references.clear();obj.fields.clear();++collected;}
 }
 return collected;
}
std::size_t Heap::liveObjects()const{
 std::lock_guard lock(mutex_);std::size_t count=0;for(const auto& obj:objects_)if(obj.bytes)++count;return count;
}
std::size_t Heap::bytesUsed()const{std::lock_guard lock(mutex_);return bytesUsed_;}
}
