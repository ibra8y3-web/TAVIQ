#include "taviq/compiler.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <unordered_map>
#include <utility>
namespace taviq {
namespace {
enum class Type { Number, String, Boolean };
struct Binding { std::uint32_t slot{}; Type type{Type::Number}; bool mutableLocal{false}; };
struct Signature {
    std::size_t index{};
    const Function* declaration{};
    std::vector<Type> parameters;
    Type result{Type::Number};
};
using Signatures=std::unordered_map<std::string,Signature>;
struct Context {
    Bytecode& bytecode;
    const Signatures& functions;
    std::vector<std::unordered_map<std::string,Binding>> scopes{{}};
    std::optional<std::size_t> functionIndex;
    std::optional<Type> returnType;
    std::vector<std::string>& localNames() {
        return functionIndex ? bytecode.functions[*functionIndex].locals : bytecode.locals;
    }
};
bool typeFromName(const std::string& name,Type& type) {
    if(name=="number"){type=Type::Number;return true;}
    if(name=="string"){type=Type::String;return true;}
    if(name=="bool"){type=Type::Boolean;return true;}
    return false;
}
ValueType toBytecodeType(Type type) {
    switch(type){
        case Type::Number:return ValueType::Number;
        case Type::String:return ValueType::String;
        case Type::Boolean:return ValueType::Boolean;
    }
    return ValueType::Number;
}
const Binding* resolve(const Context& context,const std::string& name) {
    for(auto it=context.scopes.rbegin();it!=context.scopes.rend();++it) {
        const auto found=it->find(name);
        if(found!=it->end())return &found->second;
    }
    return nullptr;
}
bool infer(const std::shared_ptr<Expr>& expression,const Context& context,
           Type& result,std::vector<std::string>& errors) {
    if(!expression){errors.push_back("missing expression");return false;}
    if(expression->kind==Expr::Kind::Number){result=Type::Number;return true;}
    if(expression->kind==Expr::Kind::String){result=Type::String;return true;}
    if(expression->kind==Expr::Kind::Boolean){result=Type::Boolean;return true;}
    if(expression->kind==Expr::Kind::Variable) {
        const auto* binding=resolve(context,expression->text);
        if(!binding){errors.push_back("undefined variable: "+expression->text);return false;}
        result=binding->type;return true;
    }
    if(expression->kind==Expr::Kind::Call) {
        if(expression->text=="len"||expression->text=="sqrt"||expression->text=="abs") {
            if(expression->arguments.size()!=1) {
                errors.push_back("built-in function '"+expression->text+"' requires exactly one argument");return false;
            }
            Type argument;
            if(!infer(expression->arguments.front(),context,argument,errors))return false;
            if(expression->text=="len"&&argument==Type::String){result=Type::Number;return true;}
            if((expression->text=="sqrt"||expression->text=="abs")&&argument==Type::Number){result=Type::Number;return true;}
            errors.push_back("invalid argument type for built-in function '"+expression->text+"'");return false;
        }
        const auto found=context.functions.find(expression->text);
        if(found==context.functions.end()){errors.push_back("unknown function: "+expression->text);return false;}
        const auto& signature=found->second;
        if(expression->arguments.size()!=signature.parameters.size()) {
            errors.push_back("function '"+expression->text+"' expects "+std::to_string(signature.parameters.size())+" argument(s)");
            return false;
        }
        for(std::size_t i=0;i<expression->arguments.size();++i) {
            Type argument;
            if(!infer(expression->arguments[i],context,argument,errors))return false;
            if(argument!=signature.parameters[i]) {
                errors.push_back("argument "+std::to_string(i+1)+" of function '"+expression->text+"' has an incompatible type");
                return false;
            }
        }
        result=signature.result;return true;
    }
    if(expression->kind==Expr::Kind::Unary) {
        Type operand;
        if(!infer(expression->right,context,operand,errors))return false;
        if(expression->op=='-'&&operand==Type::Number){result=Type::Number;return true;}
        if(expression->op=='!'&&operand==Type::Boolean){result=Type::Boolean;return true;}
        errors.push_back(expression->op=='-'?"unary '-' requires a number":"logical NOT requires a boolean");return false;
    }
    Type left,right;
    if(!infer(expression->left,context,left,errors)||!infer(expression->right,context,right,errors))return false;
    if(expression->op=='+'&&left==Type::String&&right==Type::String){result=Type::String;return true;}
    if(expression->op=='&'||expression->op=='|') {
        if(left==Type::Boolean&&right==Type::Boolean){result=Type::Boolean;return true;}
        errors.push_back("logical AND/OR requires Boolean operands");return false;
    }
    if(expression->op=='q'||expression->op=='!') {
        if(left==right){result=Type::Boolean;return true;}
        errors.push_back("equality operands must have the same type");return false;
    }
    if(expression->op=='<'||expression->op=='l'||expression->op=='>'||expression->op=='g') {
        if(left==Type::Number&&right==Type::Number){result=Type::Boolean;return true;}
        errors.push_back("ordering comparisons require numeric operands");return false;
    }
    if(left!=Type::Number||right!=Type::Number) {
        errors.push_back("arithmetic operators require numbers (only string + string is supported)");return false;
    }
    result=Type::Number;return true;
}
void emit(Context& context,OpCode op,std::uint32_t slot=0,double number=0.0,
          std::string text={},std::uint32_t argumentCount=0) {
    context.bytecode.code.push_back(Instruction{op,number,slot,std::move(text),argumentCount});
}
bool emitExpr(const std::shared_ptr<Expr>& expression,Context& context,
              std::vector<std::string>& errors);
bool compileStatements(const std::vector<Statement>& statements,Context& context,
                       std::vector<std::string>& errors,bool insideLoop);

bool emitExpr(const std::shared_ptr<Expr>& expression,Context& context,
              std::vector<std::string>& errors) {
    Type inferred;
    if(!infer(expression,context,inferred,errors))return false;
    switch(expression->kind) {
        case Expr::Kind::Number: {
            char* end=nullptr;
            const double value=std::strtod(expression->text.c_str(),&end);
            if(!end||*end!='\0'||!std::isfinite(value)) {
                errors.push_back("invalid or non-finite number literal: "+expression->text);return false;
            }
            emit(context,OpCode::PushNumber,0,value);return true;
        }
        case Expr::Kind::String:
            emit(context,OpCode::PushString,0,0.0,expression->text);return true;
        case Expr::Kind::Boolean:
            emit(context,OpCode::PushBoolean,expression->boolean?1u:0u);return true;
        case Expr::Kind::Variable: {
            const auto* binding=resolve(context,expression->text);
            if(!binding){errors.push_back("undefined variable: "+expression->text);return false;}
            emit(context,OpCode::LoadLocal,binding->slot);return true;
        }
        case Expr::Kind::Call: {
            if(expression->text=="len"||expression->text=="sqrt"||expression->text=="abs") {
                if(!emitExpr(expression->arguments.front(),context,errors))return false;
                if(expression->text=="len")emit(context,OpCode::Length);
                else if(expression->text=="sqrt")emit(context,OpCode::SquareRoot);
                else emit(context,OpCode::Absolute);
                return true;
            }
            const auto found=context.functions.find(expression->text);
            if(found==context.functions.end()){errors.push_back("unknown function: "+expression->text);return false;}
            for(const auto& argument:expression->arguments)
                if(!emitExpr(argument,context,errors))return false;
            emit(context,OpCode::Call,static_cast<std::uint32_t>(found->second.index),
                 0.0,{},static_cast<std::uint32_t>(expression->arguments.size()));
            return true;
        }
        case Expr::Kind::Unary:
            if(!emitExpr(expression->right,context,errors))return false;
            emit(context,expression->op=='-'?OpCode::Negate:OpCode::LogicalNot);return true;
        case Expr::Kind::Binary:
            if(expression->op=='&') {
                if(!emitExpr(expression->left,context,errors))return false;
                const auto branch=context.bytecode.code.size();emit(context,OpCode::JumpIfFalse);
                if(!emitExpr(expression->right,context,errors))return false;
                const auto done=context.bytecode.code.size();emit(context,OpCode::Jump);
                context.bytecode.code[branch].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                emit(context,OpCode::PushBoolean,0);
                context.bytecode.code[done].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                return true;
            }
            if(expression->op=='|') {
                if(!emitExpr(expression->left,context,errors))return false;
                const auto branch=context.bytecode.code.size();emit(context,OpCode::JumpIfFalse);
                emit(context,OpCode::PushBoolean,1);
                const auto done=context.bytecode.code.size();emit(context,OpCode::Jump);
                context.bytecode.code[branch].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                if(!emitExpr(expression->right,context,errors))return false;
                context.bytecode.code[done].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                return true;
            }
            if(!emitExpr(expression->left,context,errors)||!emitExpr(expression->right,context,errors))return false;
            switch(expression->op) {
                case '+':emit(context,OpCode::Add);break;
                case '-':emit(context,OpCode::Subtract);break;
                case '*':emit(context,OpCode::Multiply);break;
                case '/':emit(context,OpCode::Divide);break;
                case 'q':emit(context,OpCode::Equal);break;
                case '!':emit(context,OpCode::NotEqual);break;
                case '<':emit(context,OpCode::Less);break;
                case 'l':emit(context,OpCode::LessEqual);break;
                case '>':emit(context,OpCode::Greater);break;
                case 'g':emit(context,OpCode::GreaterEqual);break;
                default:errors.push_back("unsupported binary operator");return false;
            }
            return true;
    }
    return false;
}
bool definitelyReturns(const std::vector<Statement>& statements) {
    for(const auto& statement:statements) {
        if(statement.kind==Statement::Kind::Return)return true;
        if(statement.kind==Statement::Kind::If&&!statement.elseBody.empty()&&
           definitelyReturns(statement.body)&&definitelyReturns(statement.elseBody))return true;
    }
    return false;
}
bool compileBlock(const std::vector<Statement>& statements,Context& context,
                  std::vector<std::string>& errors,bool insideLoop) {
    context.scopes.emplace_back();
    const bool ok=compileStatements(statements,context,errors,insideLoop);
    context.scopes.pop_back();
    return ok;
}
bool compileStatements(const std::vector<Statement>& statements,Context& context,
                       std::vector<std::string>& errors,bool insideLoop) {
    bool ok=true;
    for(const auto& statement:statements) {
        if(statement.kind==Statement::Kind::Let||statement.kind==Statement::Kind::Var) {
            if(insideLoop&&statement.kind==Statement::Kind::Let) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": use 'var' for declarations inside a while loop; loop-local let bindings are not supported yet");
                ok=false;continue;
            }
            if(context.scopes.back().contains(statement.name)) {
                errors.push_back("line "+std::to_string(statement.line)+": duplicate variable in this scope: "+statement.name);
                ok=false;continue;
            }
            Type type;
            if(!infer(statement.expression,context,type,errors)){ok=false;continue;}
            if(!statement.declaredType.empty()) {
                Type annotated{};
                if(!typeFromName(statement.declaredType,annotated)) {
                    errors.push_back("line "+std::to_string(statement.line)+": unknown type annotation '"+statement.declaredType+"'");
                    ok=false;continue;
                }
                if(type!=annotated) {
                    errors.push_back("line "+std::to_string(statement.line)+": initializer for '"+statement.name+
                                     "' does not match declared type '"+statement.declaredType+"'");
                    ok=false;continue;
                }
                type=annotated;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            auto& names=context.localNames();
            if(names.size()>=MaxLocals){errors.push_back("QVM function local limit exceeded");return false;}
            const auto slot=static_cast<std::uint32_t>(names.size());
            names.push_back(statement.name+"#"+std::to_string(slot));
            const bool isMutable=statement.kind==Statement::Kind::Var;
            context.scopes.back().emplace(statement.name,Binding{slot,type,isMutable});
            emit(context,isMutable?OpCode::StoreLocalMutable:OpCode::StoreLocal,slot);
        } else if(statement.kind==Statement::Kind::Assign) {
            const auto* binding=resolve(context,statement.name);
            if(!binding){errors.push_back("line "+std::to_string(statement.line)+": assignment to undefined variable '"+statement.name+"'");ok=false;continue;}
            if(!binding->mutableLocal){errors.push_back("line "+std::to_string(statement.line)+": cannot assign to immutable let '"+statement.name+"'");ok=false;continue;}
            Type assigned;
            if(!infer(statement.expression,context,assigned,errors)||assigned!=binding->type) {
                errors.push_back("line "+std::to_string(statement.line)+": assignment type does not match '"+statement.name+"'");
                ok=false;continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            emit(context,OpCode::StoreLocalMutable,binding->slot);
        } else if(statement.kind==Statement::Kind::Display) {
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            emit(context,OpCode::Print);
        } else if(statement.kind==Statement::Kind::Return) {
            if(!context.returnType) {
                errors.push_back("line "+std::to_string(statement.line)+": return is only valid inside a typed function");
                ok=false;continue;
            }
            Type returned;
            if(!infer(statement.expression,context,returned,errors)||returned!=*context.returnType) {
                errors.push_back("line "+std::to_string(statement.line)+": returned value does not match function return type");
                ok=false;continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            emit(context,OpCode::Return);
        } else if(statement.kind==Statement::Kind::If) {
            Type condition;
            if(!infer(statement.expression,context,condition,errors)||condition!=Type::Boolean) {
                errors.push_back("line "+std::to_string(statement.line)+": if condition must be Boolean");ok=false;continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            const auto otherwise=context.bytecode.code.size();emit(context,OpCode::JumpIfFalse);
            if(!compileBlock(statement.body,context,errors,insideLoop))ok=false;
            if(!statement.elseBody.empty()) {
                const auto done=context.bytecode.code.size();emit(context,OpCode::Jump);
                context.bytecode.code[otherwise].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                if(!compileBlock(statement.elseBody,context,errors,insideLoop))ok=false;
                context.bytecode.code[done].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
            } else {
                context.bytecode.code[otherwise].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
            }
        } else if(statement.kind==Statement::Kind::While) {
            const auto loopHead=context.bytecode.code.size();
            Type condition;
            if(!infer(statement.expression,context,condition,errors)||condition!=Type::Boolean) {
                errors.push_back("line "+std::to_string(statement.line)+": while condition must be Boolean");ok=false;continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            const auto exit=context.bytecode.code.size();emit(context,OpCode::JumpIfFalse);
            if(!compileBlock(statement.body,context,errors,true))ok=false;
            emit(context,OpCode::Jump,static_cast<std::uint32_t>(loopHead));
            context.bytecode.code[exit].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
        }
        if(context.bytecode.code.size()>1'000'000){errors.push_back("program exceeds QVM instruction limit");return false;}
        if(context.returnType&&(statement.kind==Statement::Kind::Return||
           (statement.kind==Statement::Kind::If&&!statement.elseBody.empty()&&
            definitelyReturns(statement.body)&&definitelyReturns(statement.elseBody))))break;
    }
    return ok;
}
}
bool compile(const Program& program,Bytecode& output,std::vector<std::string>& errors) {
    errors.clear();
    Bytecode result;
    result.formatVersion=3;
    Signatures signatures;
    const auto builtin=[](const std::string& name){return name=="len"||name=="sqrt"||name=="abs";};
    for(std::size_t index=0;index<program.functions.size();++index) {
        const auto& function=program.functions[index];
        if(function.name.empty()||builtin(function.name)||signatures.contains(function.name)) {
            errors.push_back("function name is empty, duplicated or reserved: "+function.name);continue;
        }
        if(index>=MaxFunctions){errors.push_back("QVM function limit exceeded");return false;}
        Signature signature;signature.index=index;signature.declaration=&function;
        if(!typeFromName(function.returnType,signature.result)) {
            errors.push_back("function '"+function.name+"' has an unsupported return type '"+function.returnType+"'");continue;
        }
        std::unordered_map<std::string,bool> parameterNames;
        for(const auto& parameter:function.parameters) {
            Type type;
            if(parameter.name.empty()||parameterNames.contains(parameter.name)) {
                errors.push_back("function '"+function.name+"' has an empty or duplicate parameter name");break;
            }
            if(!typeFromName(parameter.declaredType,type)) {
                errors.push_back("parameter '"+parameter.name+"' of '"+function.name+"' has an unsupported type");break;
            }
            parameterNames.emplace(parameter.name,true);
            signature.parameters.push_back(type);
        }
        if(signature.parameters.size()==function.parameters.size())
            signatures.emplace(function.name,std::move(signature));
    }
    if(!errors.empty())return false;

    Context mainContext{result,signatures};
    if(!compileStatements(program.statements,mainContext,errors,false)||!errors.empty())return false;
    emit(mainContext,OpCode::Halt);
    result.mainEnd=static_cast<std::uint32_t>(result.code.size());

    result.functions.resize(program.functions.size());
    for(std::size_t index=0;index<program.functions.size();++index) {
        const auto& function=program.functions[index];
        const auto found=signatures.find(function.name);
        if(found==signatures.end())return false;
        auto& metadata=result.functions[index];
        metadata.name=function.name;
        metadata.entry=static_cast<std::uint32_t>(result.code.size());
        metadata.returnType=toBytecodeType(found->second.result);
        for(std::size_t parameter=0;parameter<function.parameters.size();++parameter) {
            metadata.parameterTypes.push_back(toBytecodeType(found->second.parameters[parameter]));
            metadata.locals.push_back(function.parameters[parameter].name+"#"+std::to_string(parameter));
        }
        Context functionContext{result,signatures};
        functionContext.functionIndex=index;
        functionContext.returnType=found->second.result;
        for(std::size_t parameter=0;parameter<function.parameters.size();++parameter) {
            functionContext.scopes.front().emplace(function.parameters[parameter].name,
                Binding{static_cast<std::uint32_t>(parameter),found->second.parameters[parameter],false});
        }
        if(!definitelyReturns(function.body)) {
            errors.push_back("function '"+function.name+"' does not return a value on every path");return false;
        }
        if(!compileStatements(function.body,functionContext,errors,false)||!errors.empty())return false;
        metadata.end=static_cast<std::uint32_t>(result.code.size());
        if(metadata.entry==metadata.end||result.code.back().op!=OpCode::Return) {
            errors.push_back("compiler did not emit a terminal RETURN for function '"+function.name+"'");return false;
        }
    }
    if(result.code.size()>1'000'000){errors.push_back("program exceeds QVM instruction limit");return false;}
    std::string verifyError;
    if(!verifyBytecode(result,verifyError)) {
        errors.push_back("compiler emitted invalid QVM v3: "+verifyError);return false;
    }
    output=std::move(result);
    return true;
}
}
