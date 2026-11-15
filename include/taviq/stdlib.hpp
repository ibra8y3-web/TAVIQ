#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>
namespace taviq::stdlib {
template<class T> class Array {
public:
    Array()=default;
    explicit Array(std::size_t reserveCount){values_.reserve(reserveCount);}
    void push(T value){values_.push_back(std::move(value));}
    std::size_t size()const noexcept{return values_.size();}
    bool empty()const noexcept{return values_.empty();}
    T& at(std::size_t index){return values_.at(index);}
    const T& at(std::size_t index)const{return values_.at(index);}
    void clear()noexcept{values_.clear();}
    auto begin()noexcept{return values_.begin();}
    auto end()noexcept{return values_.end();}
    auto begin()const noexcept{return values_.begin();}
    auto end()const noexcept{return values_.end();}
private:
    std::vector<T> values_;
};

bool readTextFile(const std::filesystem::path& root,std::string_view relativePath,
                  std::string& output,std::string& error,std::size_t maxBytes=4*1024*1024);
bool writeTextFile(const std::filesystem::path& root,std::string_view relativePath,
                   std::string_view content,std::string& error,std::size_t maxBytes=4*1024*1024);
std::string utcNowIso8601();

class BinaryWriter {
public:
    void writeU32(std::uint32_t value);
    void writeU64(std::uint64_t value);
    void writeString(std::string_view value,std::size_t maxBytes=4*1024*1024);
    const std::vector<std::uint8_t>& bytes()const noexcept{return bytes_;}
private:
    std::vector<std::uint8_t> bytes_;
};
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<std::uint8_t>& bytes):bytes_(bytes){}
    bool readU32(std::uint32_t& value)noexcept;
    bool readU64(std::uint64_t& value)noexcept;
    bool readString(std::string& value,std::size_t maxBytes=4*1024*1024);
    bool finished()const noexcept{return offset_==bytes_.size();}
private:
    const std::vector<std::uint8_t>& bytes_;
    std::size_t offset_{0};
};

struct NetworkPolicy {
    std::vector<std::string> allowedHosts;
    std::vector<std::uint16_t> allowedPorts;
    std::size_t maxRequestBytes{1024*1024};
    std::size_t maxResponseBytes{4*1024*1024};
    unsigned timeoutSeconds{5};
};
/* Plain TCP transport. TLS is intentionally not implied; use an HTTPS client for sensitive data. */
bool tcpExchange(const NetworkPolicy& policy,const std::string& host,std::uint16_t port,
                 std::string_view request,std::string& response,std::string& error);
}
