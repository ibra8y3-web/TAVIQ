#include "taviq/bytecode.hpp"
#include "taviq/lexer.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
int main(){
 taviq::Bytecode seed;
 seed.code.push_back({taviq::OpCode::PushString,0,0,"TAVIQ fuzz seed"});
 seed.code.push_back({taviq::OpCode::Print,0,0,{}});
 seed.code.push_back({taviq::OpCode::Halt,0,0,{}});
 std::ostringstream encoded(std::ios::binary);std::string error;
 if(!seed.save(encoded,error)){std::cerr<<error<<'\n';return 1;}
 const std::string original=encoded.str();std::uint32_t rng=0x71A1C0DEu;
 const auto next=[&](){rng=rng*1664525u+1013904223u;return rng;};
 for(std::size_t iteration=0;iteration<6000;++iteration){
  std::string bytes=original;const auto mutations=1+(next()%4);
  for(std::uint32_t n=0;n<mutations;++n){
   const auto index=static_cast<std::size_t>(next()%bytes.size());
   bytes[index]=static_cast<char>(static_cast<unsigned char>(bytes[index])^(1u+(next()%255u)));
  }
  if(iteration%5==0&&!bytes.empty())bytes.resize(next()%bytes.size());
  try{
   std::istringstream input(bytes,std::ios::binary);taviq::Bytecode decoded;
   if(taviq::Bytecode::load(input,decoded,error)){
    std::ostringstream output;(void)taviq::VM{}.run(decoded,output,error,256);
   }
  }catch(const std::exception& ex){std::cerr<<"QVM mutation caused exception: "<<ex.what()<<'\n';return 1;}
 }
 for(std::size_t iteration=0;iteration<2500;++iteration){
  std::string source;const auto length=next()%128;
  for(std::uint32_t n=0;n<length;++n)source.push_back(static_cast<char>(32+(next()%95)));
  try{
   taviq::Lexer lexer(source);auto tokens=lexer.scan();
   if(lexer.errors().empty()){taviq::Parser parser(tokens);(void)parser.parse();}
  }catch(const std::exception& ex){std::cerr<<"Lexer/parser fuzz caused exception: "<<ex.what()<<'\n';return 1;}
 }
 return 0;
}
