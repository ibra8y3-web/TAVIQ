#include "taviq/security.hpp"
#include "taviq/stdlib.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#define CHECK(x) do{if(!(x)){std::cerr<<"CAPABILITY CHECK failed line "<<__LINE__<<": "#x"\n";return 1;}}while(false)
int main(){
 namespace fs=std::filesystem;
 const auto root=fs::temp_directory_path()/"taviq-capability-tests";
 std::error_code ec;fs::remove_all(root,ec);fs::create_directories(root);
 taviq::security::Policy readOnly(root,taviq::security::Capability::FileRead,1024,5);
 fs::path resolved;std::string error;
 CHECK(readOnly.authorizePath("source.taviq",taviq::security::Capability::FileRead,resolved,error));
 CHECK(!readOnly.authorizePath("../outside",taviq::security::Capability::FileRead,resolved,error));
 CHECK(!readOnly.authorizePath("source.taviq",taviq::security::Capability::FileWrite,resolved,error));
 CHECK(!readOnly.authorizePath("/tmp/file",taviq::security::Capability::FileRead,resolved,error));
 CHECK(readOnly.consumeInstructions(3,error));CHECK(!readOnly.consumeInstructions(3,error));
 const auto writable=taviq::security::Capability::FileRead|taviq::security::Capability::FileWrite;
 taviq::security::Policy policy(root,writable);
 CHECK(taviq::stdlib::writeTextFile(root,"nested/hello.txt","TAVIQ",error));
 std::string value;CHECK(taviq::stdlib::readTextFile(root,"nested/hello.txt",value,error));CHECK(value=="TAVIQ");
 CHECK(!taviq::stdlib::writeTextFile(root,"../escape.txt","no",error));
 CHECK(!taviq::stdlib::readTextFile(root,"nested/hello.txt",value,error,2));
 taviq::stdlib::Array<int> array;array.push(2);array.push(4);CHECK(array.size()==2&&array.at(1)==4);
 taviq::stdlib::BinaryWriter writer;writer.writeU32(42);writer.writeU64(99);writer.writeString("TAVIQ");
 taviq::stdlib::BinaryReader reader(writer.bytes());std::uint32_t n{};std::uint64_t wide{};std::string label;
 CHECK(reader.readU32(n)&&n==42);CHECK(reader.readU64(wide)&&wide==99);
 CHECK(reader.readString(label)&&label=="TAVIQ"&&reader.finished());
 CHECK(!reader.readU32(n));
 CHECK(!taviq::stdlib::utcNowIso8601().empty());
 taviq::stdlib::NetworkPolicy network;network.allowedHosts={"127.0.0.1"};network.allowedPorts={8080};
 std::string response;
 CHECK(!taviq::stdlib::tcpExchange(network,"example.com",8080,"GET / HTTP/1.0\r\n\r\n",response,error));
 CHECK(!taviq::stdlib::tcpExchange(network,"127.0.0.1",80,"GET / HTTP/1.0\r\n\r\n",response,error));
 fs::remove_all(root,ec);return 0;
}
