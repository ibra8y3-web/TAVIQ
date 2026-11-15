#include "taviq/bytecode.hpp"
#include "taviq/compiler.hpp"
#include "taviq/compute.hpp"
#include "taviq/lexer.hpp"
#include "taviq/native.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <atomic>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
static taviq::Program parse(const std::string& source){taviq::Lexer lexer(source);auto tokens=lexer.scan();if(!lexer.errors().empty())throw std::runtime_error(lexer.errors().front());taviq::Parser parser(tokens);auto p=parser.parse();if(!parser.errors().empty())throw std::runtime_error(parser.errors().front());return p;}
#define CHECK(c) do{if(!(c)){std::cerr<<"CHECK failed "<<__FILE__<<":"<<__LINE__<<": "#c"\n";return 1;}}while(false)
int main(){
 const std::string source="flow main { let base=2+3*4; let total=(base-4)/2; display base; display total*total; display \"hello \"+\"TAVIQ\"; display len(\"taviq\"); display sqrt(81)+abs(-3); }";
 auto p=parse(source);taviq::Bytecode code;std::vector<std::string> errors;std::string error;CHECK(taviq::compile(p,code,errors));CHECK(taviq::verifyBytecode(code,error));std::ostringstream out;CHECK(taviq::VM{}.run(code,out,error)==0);CHECK(out.str()=="14\n25\nhello TAVIQ\n5\n12\n");
 std::stringstream bytes(std::ios::in|std::ios::out|std::ios::binary);CHECK(code.save(bytes,error));bytes.seekg(0);taviq::Bytecode restored;CHECK(taviq::Bytecode::load(bytes,restored,error));std::ostringstream out2;CHECK(taviq::VM{}.run(restored,out2,error)==0);CHECK(out2.str()==out.str());CHECK(taviq::disassemble(restored).find("LENGTH")!=std::string::npos);
 taviq::Bytecode bad;auto unknown=parse("flow main { display unknown; }");CHECK(!taviq::compile(unknown,bad,errors));errors.clear();auto badType=parse("flow main { display sqrt(\"bad\"); }");CHECK(!taviq::compile(badType,bad,errors));errors.clear();auto badArity=parse("flow main { display abs(1,2); }");CHECK(!taviq::compile(badArity,bad,errors));errors.clear();
 auto div=parse("flow main { display 5/0; }");CHECK(taviq::compile(div,bad,errors));std::ostringstream ignore;CHECK(taviq::VM{}.run(bad,ignore,error)!=0);CHECK(error=="division by zero");
 auto native=parse("flow main { let answer=sqrt(36)+abs(-6); display answer; display len(\"abc\"); }");std::string c;CHECK(taviq::emitC(native,c,errors));CHECK(c.find("sqrt(")!=std::string::npos);CHECK(c.find("strlen(")!=std::string::npos);
 taviq::Bytecode noHalt;noHalt.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});CHECK(!taviq::verifyBytecode(noHalt,error));
 taviq::Bytecode under;under.code.push_back({taviq::OpCode::Print,0,0,{}});under.code.push_back({taviq::OpCode::Halt,0,0,{}});CHECK(!taviq::verifyBytecode(under,error));
 std::vector<int> values(1000,0);taviq::parallelFor(0,values.size(),13,[&](std::size_t i){values[i]=static_cast<int>(i*i);},4);for(std::size_t i=0;i<values.size();++i)CHECK(values[i]==static_cast<int>(i*i));
 std::atomic<std::size_t> calls{0};taviq::parallelFor(0,0,0,[&](std::size_t){++calls;},1);CHECK(calls.load()==0);taviq::parallelFor(std::numeric_limits<std::size_t>::max()-1,std::numeric_limits<std::size_t>::max(),5,[&](std::size_t i){if(i==std::numeric_limits<std::size_t>::max()-1)++calls;},1);CHECK(calls.load()==1);
 bool propagated=false;try{taviq::parallelFor(0,64,1,[](std::size_t i){if(i==7)throw std::runtime_error("worker failure");},4);}catch(const std::runtime_error&){propagated=true;}CHECK(propagated);return 0;
}