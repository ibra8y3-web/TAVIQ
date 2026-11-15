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
static taviq::Program parse(const std::string& source){
 taviq::Lexer lexer(source);auto tokens=lexer.scan();
 if(!lexer.errors().empty())throw std::runtime_error(lexer.errors().front());
 taviq::Parser parser(tokens);auto program=parser.parse();
 if(!parser.errors().empty())throw std::runtime_error(parser.errors().front());
 return program;
}
static taviq::Bytecode compileSource(const std::string& source){
 auto program=parse(source);taviq::Bytecode code;std::vector<std::string> errors;
 if(!taviq::compile(program,code,errors))throw std::runtime_error(errors.empty()?"compile failed":errors.front());
 return code;
}
#define CHECK(c) do{if(!(c)){std::cerr<<"CHECK failed "<<__FILE__<<":"<<__LINE__<<": "#c"\n";return 1;}}while(false)
int main(){
 std::string error;
 const auto arithmetic=compileSource(
  "flow main { let base=2+3*4; let total=(base-4)/2; display base; display total*total; "
  "display \"hello \"+\"TAVIQ\"; display len(\"taviq\"); display sqrt(81)+abs(-3); }");
 CHECK(arithmetic.formatVersion==2);CHECK(taviq::verifyBytecode(arithmetic,error));
 std::ostringstream arithmeticOut;CHECK(taviq::VM{}.run(arithmetic,arithmeticOut,error)==0);
 CHECK(arithmeticOut.str()=="14\n25\nhello TAVIQ\n5\n12\n");

 const auto annotated=compileSource("flow main { let count: number=3; var label: string=\"ready\"; var active: bool=true; display count; display label; display active; }");
 std::ostringstream annotatedOut;CHECK(taviq::VM{}.run(annotated,annotatedOut,error)==0);
 CHECK(annotatedOut.str()=="3\nready\ntrue\n");

 const auto control=compileSource(
  "flow main { var total=0; var i=0; "
  "while (i < 5) { i=i+1; if (i == 3) { total=total+10; } else { total=total+i; } } "
  "display total; display true and not false; display \"same\" == \"same\"; "
  "display 3 >= 2; if (false or true) { let scoped=9; display scoped; } else { display 0; } "
  "display false and (1/0==0); display true or (1/0==0); }");
 CHECK(taviq::verifyBytecode(control,error));
 std::ostringstream controlOut;CHECK(taviq::VM{}.run(control,controlOut,error)==0);
 CHECK(controlOut.str()=="22\ntrue\ntrue\ntrue\n9\nfalse\ntrue\n");

 const auto infinite=compileSource("flow main { while (true) { } }");
 std::ostringstream infiniteOut;CHECK(taviq::VM{}.run(infinite,infiniteOut,error,100)!=0);
 CHECK(error=="QVM execution instruction budget exceeded");

 const auto shortCircuit=compileSource("flow main { display false and (1/0==0); display true or (1/0==0); }");
 std::ostringstream shortOut;CHECK(taviq::VM{}.run(shortCircuit,shortOut,error)==0);
 CHECK(shortOut.str()=="false\ntrue\n");

 const auto native=parse("flow main { let answer=sqrt(36)+abs(-6); display answer; display len(\"abc\"); }");
 std::string generatedC;std::vector<std::string> nativeErrors;
 CHECK(taviq::emitC(native,generatedC,nativeErrors));
 CHECK(generatedC.find("sqrt(")!=std::string::npos);CHECK(generatedC.find("strlen(")!=std::string::npos);
 const auto structuredNative=parse("flow main { var i=0; var total=0; while(i<4){ i=i+1; if(i==2){ total=total+10; } else { total=total+i; } } display total; display true and not false; }");
 CHECK(taviq::emitC(structuredNative,generatedC,nativeErrors));
 CHECK(generatedC.find("while (")!=std::string::npos);
 CHECK(generatedC.find("if (")!=std::string::npos);
 const auto unsupportedNative=parse("flow main { display \"a\"+\"b\"; }");
 CHECK(!taviq::emitC(unsupportedNative,generatedC,nativeErrors));
 const auto mismatchedNative=parse("flow main { let label: string=1; }");
 CHECK(!taviq::emitC(mismatchedNative,generatedC,nativeErrors));nativeErrors.clear();

 std::vector<std::string> errors;taviq::Bytecode bad;
 CHECK(!taviq::compile(parse("flow main { let fixed: number=1; fixed=2; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { let wrong: number=\"text\"; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { var wrong: bool=1; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { let unknown: integer=1; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { var value=1; value=\"text\"; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { if (1) { display 1; } }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { while (false) { let x=1; } }"),bad,errors));errors.clear();

 taviq::Bytecode noHalt;noHalt.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
 CHECK(!taviq::verifyBytecode(noHalt,error));
 taviq::Bytecode under;under.code.push_back({taviq::OpCode::Print,0,0,{}});under.code.push_back({taviq::OpCode::Halt,0,0,{}});
 CHECK(!taviq::verifyBytecode(under,error));
 taviq::Bytecode duplicateStore;duplicateStore.locals={"immutable"};
 duplicateStore.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
 duplicateStore.code.push_back({taviq::OpCode::StoreLocal,0,0,{}});
 duplicateStore.code.push_back({taviq::OpCode::PushNumber,2.0,0,{}});
 duplicateStore.code.push_back({taviq::OpCode::StoreLocal,0,0,{}});
 duplicateStore.code.push_back({taviq::OpCode::Halt,0,0,{}});
 CHECK(!taviq::verifyBytecode(duplicateStore,error));

 taviq::Bytecode memoryFanout;memoryFanout.locals={"large"};
 memoryFanout.code.push_back({taviq::OpCode::PushString,0,0,std::string(1024*1024,'x')});
 memoryFanout.code.push_back({taviq::OpCode::StoreLocal,0,0,{}});
 for(int n=0;n<16;++n)memoryFanout.code.push_back({taviq::OpCode::LoadLocal,0,0,{}});
 for(int n=0;n<16;++n)memoryFanout.code.push_back({taviq::OpCode::Print,0,0,{}});
 memoryFanout.code.push_back({taviq::OpCode::Halt,0,0,{}});
 CHECK(!taviq::verifyBytecode(memoryFanout,error));CHECK(error.find("memory")!=std::string::npos);

 taviq::Bytecode badJump;
 badJump.code.push_back({taviq::OpCode::PushBoolean,0,1,{}});
 badJump.code.push_back({taviq::OpCode::JumpIfFalse,0,99,{}});
 badJump.code.push_back({taviq::OpCode::Halt,0,0,{}});
 CHECK(!taviq::verifyBytecode(badJump,error));

 taviq::Bytecode mergeTypes;
 mergeTypes.code.push_back({taviq::OpCode::PushBoolean,0,1,{}});
 mergeTypes.code.push_back({taviq::OpCode::JumpIfFalse,0,4,{}});
 mergeTypes.code.push_back({taviq::OpCode::PushNumber,1.0,0,{}});
 mergeTypes.code.push_back({taviq::OpCode::Jump,0,5,{}});
 mergeTypes.code.push_back({taviq::OpCode::PushString,0,0,"bad"});
 mergeTypes.code.push_back({taviq::OpCode::Print,0,0,{}});
 mergeTypes.code.push_back({taviq::OpCode::Halt,0,0,{}});
 CHECK(!taviq::verifyBytecode(mergeTypes,error));

 std::stringstream v2bytes(std::ios::in|std::ios::out|std::ios::binary);
 CHECK(control.save(v2bytes,error));v2bytes.seekg(0);taviq::Bytecode restored;
 CHECK(taviq::Bytecode::load(v2bytes,restored,error));CHECK(restored.formatVersion==2);
 std::ostringstream restoredOut;CHECK(taviq::VM{}.run(restored,restoredOut,error)==0);
 CHECK(restoredOut.str()==controlOut.str());

 taviq::Bytecode legacy;legacy.formatVersion=1;
 legacy.code.push_back({taviq::OpCode::PushNumber,7.0,0,{}});
 legacy.code.push_back({taviq::OpCode::Print,0,0,{}});
 legacy.code.push_back({taviq::OpCode::Halt,0,0,{}});
 std::stringstream legacyBytes(std::ios::in|std::ios::out|std::ios::binary);
 CHECK(legacy.save(legacyBytes,error));legacyBytes.seekg(0);taviq::Bytecode legacyLoaded;
 CHECK(taviq::Bytecode::load(legacyBytes,legacyLoaded,error));CHECK(legacyLoaded.formatVersion==1);
 std::ostringstream legacyOut;CHECK(taviq::VM{}.run(legacyLoaded,legacyOut,error)==0);CHECK(legacyOut.str()=="7\n");

 std::stringstream trailing(std::ios::in|std::ios::out|std::ios::binary);
 CHECK(control.save(trailing,error));trailing.put('X');trailing.seekg(0);taviq::Bytecode trailingResult;
 CHECK(!taviq::Bytecode::load(trailing,trailingResult,error));CHECK(error.find("trailing data")!=std::string::npos);

 CHECK(!taviq::compile(parse("flow main { display unknown; }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { display sqrt(\"bad\"); }"),bad,errors));errors.clear();
 CHECK(!taviq::compile(parse("flow main { display abs(1,2); }"),bad,errors));errors.clear();
 CHECK(taviq::compile(parse("flow main { display 5/0; }"),bad,errors));
 std::ostringstream ignored;CHECK(taviq::VM{}.run(bad,ignored,error)!=0);CHECK(error=="division by zero");

 std::vector<int> values(1000,0);
 taviq::parallelFor(0,values.size(),13,[&](std::size_t i){values[i]=static_cast<int>(i*i);},4);
 for(std::size_t i=0;i<values.size();++i)CHECK(values[i]==static_cast<int>(i*i));
 std::size_t traced=0;
 std::ostringstream traceOutput;
 CHECK(taviq::VM{}.run(control,traceOutput,error,1000000,
  [&](std::size_t,const taviq::Instruction&){++traced;})==0);
 CHECK(traced>0);
 traced=0;
 CHECK(taviq::VM{}.run(control,traceOutput,error,1000000,
  [&](std::size_t,const taviq::Instruction&){++traced;throw std::runtime_error("stop trace");})!=0);
 CHECK(error.find("trace callback failed")!=std::string::npos);
 std::atomic<std::size_t> calls{0};
 taviq::parallelFor(0,0,0,[&](std::size_t){++calls;},1);CHECK(calls.load()==0);
 taviq::parallelFor(std::numeric_limits<std::size_t>::max()-1,std::numeric_limits<std::size_t>::max(),5,
  [&](std::size_t i){if(i==std::numeric_limits<std::size_t>::max()-1)++calls;},1);
 CHECK(calls.load()==1);
 bool propagated=false;
 try{taviq::parallelFor(0,64,1,[](std::size_t i){if(i==7)throw std::runtime_error("worker failure");},4);}
 catch(const std::runtime_error&){propagated=true;}
 CHECK(propagated);
 return 0;
}
