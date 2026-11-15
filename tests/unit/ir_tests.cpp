#include "taviq/ir.hpp"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"IR CHECK failed line "<<__LINE__<<": "#x"\n";return 1;}}while(false)
int main(){
 using namespace taviq::ir;
 Module m;Function fn;fn.name="constant_fold";fn.returnType=Type::Number;
 Block b;b.id=0;
 b.instructions.push_back({Op::ConstantNumber,1,Type::Number,0,0,20.0,false});
 b.instructions.push_back({Op::ConstantNumber,2,Type::Number,0,0,22.0,false});
 b.instructions.push_back({Op::Add,3,Type::Number,1,2,0,false});
 b.terminator={TerminatorKind::Return,3,0,0};fn.blocks.push_back(b);m.functions.push_back(fn);
 std::string error;CHECK(verify(m,error));optimize(m);
 CHECK(m.functions[0].blocks[0].instructions[2].op==Op::ConstantNumber);
 CHECK(m.functions[0].blocks[0].instructions[2].number==42.0);
 CHECK(dump(m).find("constant_fold")!=std::string::npos);
 m.functions[0].blocks[0].instructions[2].left=999;
 m.functions[0].blocks[0].instructions[2].op=Op::Add;
 CHECK(!verify(m,error));return 0;
}
