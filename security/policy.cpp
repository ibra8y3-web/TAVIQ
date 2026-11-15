#include "taviq/security.hpp"
#include <algorithm>
#include <system_error>
namespace taviq::security {
namespace{
bool inside(const std::filesystem::path& root,const std::filesystem::path& target){
 auto r=root.begin(),t=target.begin();
 for(;r!=root.end();++r,++t)if(t==target.end()||*r!=*t)return false;
 return true;
}
}
Policy::Policy(std::filesystem::path workspace,Capability capabilities,
               std::size_t maxFileBytes,std::size_t instructionBudget)
 :workspace_(std::filesystem::weakly_canonical(std::move(workspace))),
  capabilities_(capabilities),maxFileBytes_(maxFileBytes),remainingInstructions_(instructionBudget){}
bool Policy::authorizePath(std::string_view relative,Capability operation,
                           std::filesystem::path& resolved,std::string& error)const{
 error.clear();resolved.clear();
 if(operation!=Capability::FileRead&&operation!=Capability::FileWrite){
  error="path authorization accepts only file capabilities";return false;
 }
 if(!allows(operation)){error="requested file capability was not granted";return false;}
 if(relative.empty()){error="path is empty";return false;}
 const std::filesystem::path input{std::string(relative)};
 if(input.is_absolute()||input.has_root_name()||input.has_root_directory()){
  error="absolute paths are denied";return false;
 }
 for(const auto& part:input)if(part==".."||part=="."){error="path traversal components are denied";return false;}
 try{
  const auto candidate=std::filesystem::weakly_canonical(workspace_/input);
  if(!inside(workspace_,candidate)||candidate==workspace_){error="path escapes workspace or names its root";return false;}
  if(std::filesystem::exists(candidate)&&std::filesystem::is_symlink(candidate)){
   error="symbolic-link targets are denied";return false;
  }
  resolved=candidate;return true;
 }catch(const std::filesystem::filesystem_error& ex){error=ex.what();return false;}
}
bool Policy::consumeInstructions(std::size_t count,std::string& error)noexcept{
 error.clear();auto remaining=remainingInstructions_.load(std::memory_order_relaxed);
 for(;;){
  if(count>remaining){error="sandbox instruction budget exhausted";return false;}
  if(remainingInstructions_.compare_exchange_weak(remaining,remaining-count,
      std::memory_order_relaxed,std::memory_order_relaxed))return true;
 }
}
}
