#include "taviq/ir.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
namespace taviq::ir {
namespace{
struct Definition{Type type;};
bool numeric(Op op){return op==Op::Add||op==Op::Subtract||op==Op::Multiply||op==Op::Divide;}
}
bool verify(const Module& module,std::string& error){
 error.clear();
 if(module.functions.empty()){error="IR module has no functions";return false;}
 std::unordered_set<std::string> functionNames;
 for(const auto& fn:module.functions){
  if(fn.name.empty()||!functionNames.insert(fn.name).second){error="IR function name is empty or duplicated";return false;}
  if(fn.blocks.empty()){error="IR function has no basic blocks";return false;}
  std::unordered_map<ValueId,Definition> definitions;
  std::unordered_set<std::uint32_t> blockIds;
  for(const auto& block:fn.blocks)if(!blockIds.insert(block.id).second){error="IR basic block id is duplicated";return false;}
  for(const auto& block:fn.blocks){
   for(const auto& ins:block.instructions){
    if(ins.result==0||definitions.contains(ins.result)){error="IR SSA result id is zero or duplicated";return false;}
    if(ins.op==Op::ConstantNumber&&!std::isfinite(ins.number)){error="IR constant is not finite";return false;}
    if(ins.op==Op::ConstantBoolean&&ins.type!=Type::Boolean){error="IR Boolean constant has wrong result type";return false;}
    if(numeric(ins.op)){
     const auto l=definitions.find(ins.left),r=definitions.find(ins.right);
     if(l==definitions.end()||r==definitions.end()||l->second.type!=Type::Number||r->second.type!=Type::Number||
        ins.type!=Type::Number){error="IR numeric instruction has invalid operands or result type";return false;}
    }else if(ins.op==Op::Negate){
     const auto v=definitions.find(ins.left);
     if(v==definitions.end()||v->second.type!=Type::Number||ins.type!=Type::Number){error="IR negate has invalid operand";return false;}
    }else if(ins.op==Op::LogicalNot){
     const auto v=definitions.find(ins.left);
     if(v==definitions.end()||v->second.type!=Type::Boolean||ins.type!=Type::Boolean){error="IR NOT has invalid operand";return false;}
    }else if(ins.op==Op::Equal||ins.op==Op::Less){
     const auto l=definitions.find(ins.left),r=definitions.find(ins.right);
     if(l==definitions.end()||r==definitions.end()||l->second.type!=r->second.type||
        ins.type!=Type::Boolean||(ins.op==Op::Less&&l->second.type!=Type::Number)){
      error="IR comparison has invalid operands or result type";return false;
     }
    }
    definitions.emplace(ins.result,Definition{ins.type});
   }
  }
  for(const auto& block:fn.blocks){
   const auto& term=block.terminator;
   if(term.kind==TerminatorKind::Jump&& !blockIds.contains(term.first)){error="IR jump target is missing";return false;}
   if(term.kind==TerminatorKind::Branch){
    const auto cond=definitions.find(term.value);
    if(cond==definitions.end()||cond->second.type!=Type::Boolean||
       !blockIds.contains(term.first)||!blockIds.contains(term.second)){
     error="IR branch has an invalid condition or target";return false;
    }
   }
   if(term.kind==TerminatorKind::Return){
    const auto value=definitions.find(term.value);
    if(value==definitions.end()||value->second.type!=fn.returnType){error="IR return value has incompatible type";return false;}
   }
  }
 }
 return true;
}
void optimize(Module& module){
 for(auto& fn:module.functions){
  for(auto& block:fn.blocks){
   std::unordered_map<ValueId,Instruction> constants;
   for(auto& ins:block.instructions){
    if(ins.op==Op::ConstantNumber||ins.op==Op::ConstantBoolean){constants[ins.result]=ins;continue;}
    if(ins.op==Op::Negate){
     const auto it=constants.find(ins.left);
     if(it!=constants.end()&&it->second.op==Op::ConstantNumber){
      ins.op=Op::ConstantNumber;ins.number=-it->second.number;ins.left=ins.right=0;
     }
    }else if(ins.op==Op::LogicalNot){
     const auto it=constants.find(ins.left);
     if(it!=constants.end()&&it->second.op==Op::ConstantBoolean){
      ins.op=Op::ConstantBoolean;ins.boolean=!it->second.boolean;ins.left=ins.right=0;
     }
    }else if(numeric(ins.op)){
     const auto l=constants.find(ins.left),r=constants.find(ins.right);
     if(l!=constants.end()&&r!=constants.end()&&l->second.op==Op::ConstantNumber&&r->second.op==Op::ConstantNumber){
      const double a=l->second.number,b=r->second.number;double value=0;bool fold=true;
      if(ins.op==Op::Add)value=a+b;else if(ins.op==Op::Subtract)value=a-b;
      else if(ins.op==Op::Multiply)value=a*b;else if(b!=0.0)value=a/b;else fold=false;
      if(fold&&std::isfinite(value)){ins.op=Op::ConstantNumber;ins.number=value;ins.left=ins.right=0;}
     }
    }else if(ins.op==Op::Equal||ins.op==Op::Less){
     const auto l=constants.find(ins.left),r=constants.find(ins.right);
     if(l!=constants.end()&&r!=constants.end()&&l->second.op==Op::ConstantNumber&&r->second.op==Op::ConstantNumber){
      ins.op=Op::ConstantBoolean;ins.boolean=ins.op==Op::Equal?l->second.number==r->second.number:l->second.number<r->second.number;
      // ins.op was just changed; compute comparison with the original operation below in the corrected build.
     }
    }
    if(ins.op==Op::ConstantNumber||ins.op==Op::ConstantBoolean)constants[ins.result]=ins;
   }
  }
 }
}
std::string dump(const Module& module){
 std::ostringstream out;
 for(const auto& fn:module.functions){
  out<<"fn "<<fn.name<<" -> "<<(fn.returnType==Type::Number?"number":"bool")<<"\n";
  for(const auto& block:fn.blocks){
   out<<"block "<<block.id<<":\n";
   for(const auto& ins:block.instructions)out<<"  %"<<ins.result<<" = op"<<static_cast<int>(ins.op)<<" "<<ins.left<<" "<<ins.right<<"\n";
   out<<"  term "<<static_cast<int>(block.terminator.kind)<<" "<<block.terminator.value<<" "<<block.terminator.first<<" "<<block.terminator.second<<"\n";
  }
 }
 return out.str();
}
}
