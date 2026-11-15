#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
namespace taviq::memory {
using ObjectId=std::uint64_t;
enum class Kind:std::uint8_t{String,Array,Record};
struct ObjectView{Kind kind;std::string text;std::vector<ObjectId> references;std::map<std::string,ObjectId> fields;};
class Heap {
public:
 class Root{
 public:
  Root()=default;Root(Heap* heap,ObjectId id):heap_(heap),id_(id){}
  ~Root();Root(const Root&)=delete;Root& operator=(const Root&)=delete;
  Root(Root&& other)noexcept;Root& operator=(Root&& other)noexcept;
  ObjectId id()const noexcept{return id_;}
  void reset()noexcept;
 private:Heap* heap_{nullptr};ObjectId id_{0};
 };
 explicit Heap(std::size_t byteLimit=64*1024*1024,std::size_t objectLimit=1'000'000);
 ObjectId makeString(std::string value);
 ObjectId makeArray(std::vector<ObjectId> values);
 ObjectId makeRecord(std::map<std::string,ObjectId> values);
 Root root(ObjectId id);
 bool inspect(ObjectId id,ObjectView& out)const;
 std::size_t collect();
 std::size_t liveObjects()const;
 std::size_t bytesUsed()const;
private:
 struct Object{Kind kind;std::string text;std::vector<ObjectId> references;std::map<std::string,ObjectId> fields;bool marked{false};std::size_t bytes{};};
 void unroot(ObjectId id)noexcept;
 ObjectId allocate(Object object);
 mutable std::mutex mutex_;
 std::vector<Object> objects_;
 std::map<ObjectId,std::size_t> roots_;
 std::size_t bytesUsed_{0},byteLimit_,objectLimit_;
 friend class Root;
};
}
