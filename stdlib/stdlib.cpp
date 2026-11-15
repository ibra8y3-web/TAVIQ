#include "taviq/stdlib.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <system_error>
#include <ctime>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
namespace taviq::stdlib {
namespace {
std::filesystem::path safePath(const std::filesystem::path& root,std::string_view relative){
 if(relative.empty())throw std::invalid_argument("relative path is empty");
 const std::filesystem::path rel{std::string(relative)};
 if(rel.is_absolute()||rel.has_root_name()||rel.has_root_directory())
  throw std::invalid_argument("absolute paths are not permitted");
 for(const auto& part:rel)if(part==".."||part==".")throw std::invalid_argument("dot path components are not permitted");
 const auto base=std::filesystem::weakly_canonical(root);
 const auto result=std::filesystem::weakly_canonical(base/rel);
 auto b=base.begin(),r=result.begin();
 for(;b!=base.end();++b,++r)if(r==result.end()||*b!=*r)
  throw std::invalid_argument("path escapes the workspace");
 return result;
}
#if defined(_WIN32)
struct SocketRuntime{
 WSADATA data{};bool ready{false};
 SocketRuntime(){ready=WSAStartup(MAKEWORD(2,2),&data)==0;}
 ~SocketRuntime(){if(ready)WSACleanup();}
};
using Socket=SOCKET;
constexpr Socket InvalidSocket=INVALID_SOCKET;
void closeSocket(Socket s){if(s!=InvalidSocket)closesocket(s);}
#else
using Socket=int;
constexpr Socket InvalidSocket=-1;
void closeSocket(Socket s){if(s!=InvalidSocket)::close(s);}
#endif
}
bool readTextFile(const std::filesystem::path& root,std::string_view relative,
                  std::string& output,std::string& error,std::size_t maxBytes){
 output.clear();error.clear();
 try{
  const auto path=safePath(root,relative);
  if(!std::filesystem::is_regular_file(path)){error="file does not exist or is not a regular file";return false;}
  const auto size=std::filesystem::file_size(path);
  if(size>maxBytes){error="file exceeds configured size limit";return false;}
  std::ifstream in(path,std::ios::binary);
  if(!in){error="unable to open file for reading";return false;}
  output.resize(static_cast<std::size_t>(size));
  if(size&&!in.read(output.data(),static_cast<std::streamsize>(size))){output.clear();error="short read";return false;}
  return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
bool writeTextFile(const std::filesystem::path& root,std::string_view relative,
                   std::string_view content,std::string& error,std::size_t maxBytes){
 error.clear();
 if(content.size()>maxBytes){error="content exceeds configured size limit";return false;}
 try{
  const auto path=safePath(root,relative);
  std::filesystem::create_directories(path.parent_path());
  const auto temporary=path.string()+".taviq-part";
  {
   std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
   if(!out){error="unable to open temporary file";return false;}
   out.write(content.data(),static_cast<std::streamsize>(content.size()));
   out.flush();
   if(!out){error="failed writing temporary file";return false;}
  }
  std::error_code ec;
  std::filesystem::rename(temporary,path,ec);
  if(ec){
   std::filesystem::remove(path,ec);ec.clear();
   std::filesystem::rename(temporary,path,ec);
  }
  if(ec){std::filesystem::remove(temporary);error="atomic replace failed: "+ec.message();return false;}
  return true;
 }catch(const std::exception& ex){error=ex.what();return false;}
}
std::string utcNowIso8601(){
 const auto now=std::chrono::system_clock::now();
 const std::time_t time=std::chrono::system_clock::to_time_t(now);
 std::tm tm{};
#if defined(_WIN32)
 gmtime_s(&tm,&time);
#else
 gmtime_r(&time,&tm);
#endif
 char buffer[32]{};
 if(std::strftime(buffer,sizeof(buffer),"%Y-%m-%dT%H:%M:%SZ",&tm)==0)return {};
 return buffer;
}
void BinaryWriter::writeU32(std::uint32_t value){for(unsigned i=0;i<4;++i)bytes_.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
void BinaryWriter::writeU64(std::uint64_t value){for(unsigned i=0;i<8;++i)bytes_.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
void BinaryWriter::writeString(std::string_view value,std::size_t maxBytes){
 if(value.size()>maxBytes||value.size()>std::numeric_limits<std::uint32_t>::max())throw std::length_error("serialized string exceeds limit");
 writeU32(static_cast<std::uint32_t>(value.size()));
 bytes_.insert(bytes_.end(),value.begin(),value.end());
}
bool BinaryReader::readU32(std::uint32_t& value)noexcept{
 if(bytes_.size()-offset_<4)return false;value=0;
 for(unsigned i=0;i<4;++i)value|=static_cast<std::uint32_t>(bytes_[offset_++])<<(i*8);
 return true;
}
bool BinaryReader::readU64(std::uint64_t& value)noexcept{
 if(bytes_.size()-offset_<8)return false;value=0;
 for(unsigned i=0;i<8;++i)value|=static_cast<std::uint64_t>(bytes_[offset_++])<<(i*8);
 return true;
}
bool BinaryReader::readString(std::string& value,std::size_t maxBytes){
 std::uint32_t size{};if(!readU32(size)||size>maxBytes||size>bytes_.size()-offset_)return false;
 value.assign(reinterpret_cast<const char*>(bytes_.data()+offset_),size);offset_+=size;return true;
}
bool tcpExchange(const NetworkPolicy& policy,const std::string& host,std::uint16_t port,
                 std::string_view request,std::string& response,std::string& error){
 response.clear();error.clear();
 if(std::find(policy.allowedHosts.begin(),policy.allowedHosts.end(),host)==policy.allowedHosts.end()){
  error="network host is not allowed by capability policy";return false;
 }
 if(std::find(policy.allowedPorts.begin(),policy.allowedPorts.end(),port)==policy.allowedPorts.end()){
  error="network port is not allowed by capability policy";return false;
 }
 if(request.empty()||request.size()>policy.maxRequestBytes){error="network request is empty or exceeds limit";return false;}
 if(policy.maxResponseBytes==0||policy.maxResponseBytes>64*1024*1024||policy.timeoutSeconds==0||policy.timeoutSeconds>60){
  error="network policy limits are invalid";return false;
 }
#if defined(_WIN32)
 SocketRuntime runtime;if(!runtime.ready){error="Winsock initialization failed";return false;}
#endif
 addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
 addrinfo* results=nullptr;
 const auto service=std::to_string(port);
 const int lookup=getaddrinfo(host.c_str(),service.c_str(),&hints,&results);
 if(lookup!=0){error="DNS resolution failed";return false;}
 struct AddrGuard{addrinfo* p;~AddrGuard(){if(p)freeaddrinfo(p);}} guard{results};
 Socket socketFd=InvalidSocket;
 for(auto* item=results;item;item=item->ai_next){
  Socket candidate=::socket(item->ai_family,item->ai_socktype,item->ai_protocol);
  if(candidate==InvalidSocket)continue;
  timeval timeout{};timeout.tv_sec=static_cast<long>(policy.timeoutSeconds);
  setsockopt(candidate,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
  setsockopt(candidate,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
  if(::connect(candidate,item->ai_addr,static_cast<int>(item->ai_addrlen))==0){socketFd=candidate;break;}
  closeSocket(candidate);
 }
 if(socketFd==InvalidSocket){error="TCP connection failed";return false;}
 struct SocketGuard{Socket s;~SocketGuard(){closeSocket(s);}} socketGuard{socketFd};
 std::size_t sent=0;
 while(sent<request.size()){
#if defined(_WIN32)
  const int n=::send(socketFd,request.data()+sent,static_cast<int>(request.size()-sent),0);
#else
  const auto n=::send(socketFd,request.data()+sent,request.size()-sent,0);
#endif
  if(n<=0){error="TCP send failed";return false;}sent+=static_cast<std::size_t>(n);
 }
 response.reserve(std::min<std::size_t>(policy.maxResponseBytes,8192));
 char buffer[8192];
 while(response.size()<policy.maxResponseBytes){
  const auto wanted=std::min(sizeof(buffer),policy.maxResponseBytes-response.size());
#if defined(_WIN32)
  const int n=::recv(socketFd,buffer,static_cast<int>(wanted),0);
#else
  const auto n=::recv(socketFd,buffer,wanted,0);
#endif
  if(n==0)break;
  if(n<0){error="TCP receive failed or timed out";response.clear();return false;}
  response.append(buffer,static_cast<std::size_t>(n));
 }
 if(response.size()==policy.maxResponseBytes){char probe{};const auto n=::recv(socketFd,&probe,1,0);if(n>0){response.clear();error="TCP response exceeds configured limit";return false;}}
 return true;
}
}
