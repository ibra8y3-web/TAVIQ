#pragma once
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
namespace taviq::security {
enum class Capability:unsigned{None=0,FileRead=1,FileWrite=2,Network=4,Process=8,ProviderLoad=16};
constexpr Capability operator|(Capability a,Capability b){
 return static_cast<Capability>(static_cast<unsigned>(a)|static_cast<unsigned>(b));
}
constexpr bool contains(Capability set,Capability cap){
 return (static_cast<unsigned>(set)&static_cast<unsigned>(cap))==static_cast<unsigned>(cap);
}
class Policy{
public:
 Policy(std::filesystem::path workspace,Capability capabilities,
        std::size_t maxFileBytes=4*1024*1024,std::size_t instructionBudget=1'000'000);
 bool authorizePath(std::string_view relativePath,Capability operation,
                    std::filesystem::path& resolved,std::string& error)const;
 bool consumeInstructions(std::size_t count,std::string& error)noexcept;
 bool allows(Capability capability)const noexcept{return contains(capabilities_,capability);}
 const std::filesystem::path& workspace()const noexcept{return workspace_;}
 std::size_t maxFileBytes()const noexcept{return maxFileBytes_;}
private:
 std::filesystem::path workspace_;
 Capability capabilities_;
 std::size_t maxFileBytes_;
 std::atomic<std::size_t> remainingInstructions_;
};
}
