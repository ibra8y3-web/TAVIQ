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
struct FunctionSignature {
    const Function* declaration{};
    std::vector<Type> parameterTypes;
    Type returnType{Type::Number};
};
struct Context {
    Bytecode& bytecode;
    std::vector<std::unordered_map<std::string,Binding>> scopes{{}};
    std::unordered_map<std::string,FunctionSignature> functions;
    std::vector<std::string> expansionStack;
    std::optional<Type> returnType;
    std::vector<std::size_t> returnJumps;
    bool inInlineFunction{false};
};

bool resolveTypeName(const std::string& name,Type& type) {
    if(name=="number"){type=Type::Number;return true;}
    if(name=="string"){type=Type::String;return true;}
    if(name=="bool"){type=Type::Boolean;return true;}
    return false;
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
        result=binding->type;
        return true;
    }
    if(expression->kind==Expr::Kind::Call) {
        if(expression->text=="len"||expression->text=="sqrt"||expression->text=="abs") {
            if(expression->arguments.size()!=1) {
                errors.push_back("built-in function '"+expression->text+"' requires exactly one argument");
                return false;
            }
            Type argument;
            if(!infer(expression->arguments.front(),context,argument,errors))return false;
            if(expression->text=="len"&&argument==Type::String){result=Type::Number;return true;}
            if((expression->text=="sqrt"||expression->text=="abs")&&argument==Type::Number){result=Type::Number;return true;}
            errors.push_back("invalid argument type for built-in function '"+expression->text+"'");
            return false;
        }
        const auto found=context.functions.find(expression->text);
        if(found==context.functions.end()) {
            errors.push_back("unknown function: "+expression->text);
            return false;
        }
        const auto& signature=found->second;
        if(expression->arguments.size()!=signature.parameterTypes.size()) {
            errors.push_back("function '"+expression->text+"' expects "+
                std::to_string(signature.parameterTypes.size())+" argument(s)");
            return false;
        }
        for(std::size_t i=0;i<expression->arguments.size();++i) {
            Type argument;
            if(!infer(expression->arguments[i],context,argument,errors))return false;
            if(argument!=signature.parameterTypes[i]) {
                errors.push_back("argument "+std::to_string(i+1)+" of function '"+
                    expression->text+"' has an incompatible type");
                return false;
            }
        }
        result=signature.returnType;
        return true;
    }
    if(expression->kind==Expr::Kind::Unary) {
        Type operand;
        if(!infer(expression->right,context,operand,errors))return false;
        if(expression->op=='-'&&operand==Type::Number){result=Type::Number;return true;}
        if(expression->op=='!'&&operand==Type::Boolean){result=Type::Boolean;return true;}
        errors.push_back(expression->op=='-'?"unary '-' requires a number":"logical NOT requires a boolean");
        return false;
    }
    Type left,right;
    if(!infer(expression->left,context,left,errors)||!infer(expression->right,context,right,errors))return false;
    if(expression->op=='+'&&left==Type::String&&right==Type::String){result=Type::String;return true;}
    if(expression->op=='&'||expression->op=='|') {
        if(left==Type::Boolean&&right==Type::Boolean){result=Type::Boolean;return true;}
        errors.push_back("logical AND/OR requires boolean operands");
        return false;
    }
    if(expression->op=='q'||expression->op=='!') {
        if(left==right){result=Type::Boolean;return true;}
        errors.push_back("equality operands must have the same type");
        return false;
    }
    if(expression->op=='<'||expression->op=='l'||expression->op=='>'||expression->op=='g') {
        if(left==Type::Number&&right==Type::Number){result=Type::Boolean;return true;}
        errors.push_back("ordering comparisons require numeric operands");
        return false;
    }
    if(left!=Type::Number||right!=Type::Number) {
        errors.push_back("arithmetic operators require numbers (only string + string is supported)");
        return false;
    }
    result=Type::Number;
    return true;
}
void emit(Context& context,OpCode opcode,std::uint32_t slot=0,
          double number=0.0,std::string text={}) {
    context.bytecode.code.push_back(Instruction{opcode,number,slot,std::move(text)});
}
bool emitExpr(const std::shared_ptr<Expr>& expression,Context& context,
              std::vector<std::string>& errors);
bool compileStatements(const std::vector<Statement>& statements,Context& context,
                       std::vector<std::string>& errors,bool insideLoop);
bool emitInlineFunction(const std::shared_ptr<Expr>& call,Context& context,
                        std::vector<std::string>& errors);

bool emitExpr(const std::shared_ptr<Expr>& expression,Context& context,
              std::vector<std::string>& errors) {
    Type type;
    if(!infer(expression,context,type,errors))return false;
    switch(expression->kind) {
        case Expr::Kind::Number: {
            char* end=nullptr;
            const double value=std::strtod(expression->text.c_str(),&end);
            if(!end||*end!='\0'||!std::isfinite(value)) {
                errors.push_back("invalid or non-finite number literal: "+expression->text);
                return false;
            }
            emit(context,OpCode::PushNumber,0,value);
            return true;
        }
        case Expr::Kind::String:
            emit(context,OpCode::PushString,0,0.0,expression->text);
            return true;
        case Expr::Kind::Boolean:
            emit(context,OpCode::PushBoolean,expression->boolean?1u:0u);
            return true;
        case Expr::Kind::Variable: {
            const auto* binding=resolve(context,expression->text);
            if(!binding){errors.push_back("undefined variable: "+expression->text);return false;}
            emit(context,OpCode::LoadLocal,binding->slot);
            return true;
        }
        case Expr::Kind::Call:
            if(expression->text=="len"||expression->text=="sqrt"||expression->text=="abs") {
                if(!emitExpr(expression->arguments.front(),context,errors))return false;
                if(expression->text=="len")emit(context,OpCode::Length);
                else if(expression->text=="sqrt")emit(context,OpCode::SquareRoot);
                else emit(context,OpCode::Absolute);
                return true;
            }
            return emitInlineFunction(expression,context,errors);
        case Expr::Kind::Unary:
            if(!emitExpr(expression->right,context,errors))return false;
            emit(context,expression->op=='-'?OpCode::Negate:OpCode::LogicalNot);
            return true;
        case Expr::Kind::Binary:
            if(expression->op=='&') {
                if(!emitExpr(expression->left,context,errors))return false;
                const auto branch=context.bytecode.code.size();
                emit(context,OpCode::JumpIfFalse);
                if(!emitExpr(expression->right,context,errors))return false;
                const auto done=context.bytecode.code.size();
                emit(context,OpCode::Jump);
                context.bytecode.code[branch].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                emit(context,OpCode::PushBoolean,0);
                context.bytecode.code[done].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                return true;
            }
            if(expression->op=='|') {
                if(!emitExpr(expression->left,context,errors))return false;
                const auto branch=context.bytecode.code.size();
                emit(context,OpCode::JumpIfFalse);
                emit(context,OpCode::PushBoolean,1);
                const auto done=context.bytecode.code.size();
                emit(context,OpCode::Jump);
                context.bytecode.code[branch].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                if(!emitExpr(expression->right,context,errors))return false;
                context.bytecode.code[done].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
                return true;
            }
            if(!emitExpr(expression->left,context,errors)||
               !emitExpr(expression->right,context,errors))return false;
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
                ok=false;
                continue;
            }
            if(context.scopes.back().contains(statement.name)) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": variable already declared in this scope: "+statement.name);
                ok=false;
                continue;
            }
            Type type;
            if(!infer(statement.expression,context,type,errors)){ok=false;continue;}
            if(!statement.declaredType.empty()) {
                Type annotated=Type::Number;
                if(!resolveTypeName(statement.declaredType,annotated)) {
                    errors.push_back("line "+std::to_string(statement.line)+
                        ": unknown type annotation '"+statement.declaredType+"' (use number, string or bool)");
                    ok=false;
                    continue;
                }
                if(type!=annotated) {
                    errors.push_back("line "+std::to_string(statement.line)+
                        ": initializer for '"+statement.name+"' does not match declared type '"+
                        statement.declaredType+"'");
                    ok=false;
                    continue;
                }
                type=annotated;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            if(context.bytecode.locals.size()>=65'536) {
                errors.push_back("QVM local limit exceeded");
                return false;
            }
            const auto slot=static_cast<std::uint32_t>(context.bytecode.locals.size());
            context.bytecode.locals.push_back(statement.name+"#"+std::to_string(slot));
            const bool mutableLocal=statement.kind==Statement::Kind::Var;
            context.scopes.back().emplace(statement.name,Binding{slot,type,mutableLocal});
            const auto store=mutableLocal||context.inInlineFunction?
                OpCode::StoreLocalMutable:OpCode::StoreLocal;
            emit(context,store,slot);
        } else if(statement.kind==Statement::Kind::Assign) {
            const auto* binding=resolve(context,statement.name);
            if(!binding) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": assignment to undefined variable: "+statement.name);
                ok=false;
                continue;
            }
            if(!binding->mutableLocal) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": cannot assign to immutable let variable: "+statement.name);
                ok=false;
                continue;
            }
            Type type;
            if(!infer(statement.expression,context,type,errors)||type!=binding->type) {
                if(errors.empty()||errors.back().find("assignment type")==std::string::npos)
                    errors.push_back("line "+std::to_string(statement.line)+
                        ": assignment type must match variable '"+statement.name+"'");
                ok=false;
                continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            emit(context,OpCode::StoreLocalMutable,binding->slot);
        } else if(statement.kind==Statement::Kind::Return) {
            if(!context.returnType) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": return is only valid inside a typed function");
                ok=false;
                continue;
            }
            Type returned;
            if(!infer(statement.expression,context,returned,errors)||returned!=*context.returnType) {
                errors.push_back("line "+std::to_string(statement.line)+
                    ": return value does not match the function return type");
                ok=false;
                continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            context.returnJumps.push_back(context.bytecode.code.size());
            emit(context,OpCode::Jump);
        } else if(statement.kind==Statement::Kind::Display) {
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            emit(context,OpCode::Print);
        } else if(statement.kind==Statement::Kind::If) {
            Type condition;
            if(!infer(statement.expression,context,condition,errors)||condition!=Type::Boolean) {
                errors.push_back("line "+std::to_string(statement.line)+": if condition must be boolean");
                ok=false;
                continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            const auto otherwise=context.bytecode.code.size();
            emit(context,OpCode::JumpIfFalse);
            if(!compileBlock(statement.body,context,errors,insideLoop))ok=false;
            if(!statement.elseBody.empty()) {
                const auto done=context.bytecode.code.size();
                emit(context,OpCode::Jump);
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
                errors.push_back("line "+std::to_string(statement.line)+": while condition must be boolean");
                ok=false;
                continue;
            }
            if(!emitExpr(statement.expression,context,errors)){ok=false;continue;}
            const auto exit=context.bytecode.code.size();
            emit(context,OpCode::JumpIfFalse);
            if(!compileBlock(statement.body,context,errors,true))ok=false;
            emit(context,OpCode::Jump,static_cast<std::uint32_t>(loopHead));
            context.bytecode.code[exit].slot=static_cast<std::uint32_t>(context.bytecode.code.size());
        }
        if(context.bytecode.code.size()>1'000'000) {
            errors.push_back("program exceeds QVM instruction limit");
            return false;
        }
    }
    return ok;
}

bool blockDefinitelyReturns(const std::vector<Statement>& statements) {
    for(const auto& statement:statements) {
        if(statement.kind==Statement::Kind::Return)return true;
        if(statement.kind==Statement::Kind::If&&!statement.elseBody.empty()&&
           blockDefinitelyReturns(statement.body)&&blockDefinitelyReturns(statement.elseBody))
            return true;
    }
    return false;
}

bool emitInlineFunction(const std::shared_ptr<Expr>& call,Context& context,
                        std::vector<std::string>& errors) {
    Type callType;
    if(!infer(call,context,callType,errors))return false;
    const auto found=context.functions.find(call->text);
    if(found==context.functions.end()) {
        errors.push_back("unknown function: "+call->text);
        return false;
    }
    const auto& signature=found->second;
    if(std::find(context.expansionStack.begin(),context.expansionStack.end(),call->text)!=context.expansionStack.end()) {
        errors.push_back("recursive function calls are not supported by the current static-expansion backend: "+call->text);
        return false;
    }
    if(!blockDefinitelyReturns(signature.declaration->body)) {
        errors.push_back("function '"+call->text+"' must return a value on every control-flow path");
        return false;
    }

    std::vector<std::uint32_t> temporarySlots;
    for(std::size_t i=0;i<call->arguments.size();++i) {
        if(!emitExpr(call->arguments[i],context,errors))return false;
        if(context.bytecode.locals.size()>=65'536) {
            errors.push_back("QVM local limit exceeded while expanding function arguments");
            return false;
        }
        const auto slot=static_cast<std::uint32_t>(context.bytecode.locals.size());
        context.bytecode.locals.push_back("$taviq_arg_"+std::to_string(slot));
        temporarySlots.push_back(slot);
        emit(context,OpCode::StoreLocalMutable,slot);
    }

    auto callerScopes=std::move(context.scopes);
    auto callerReturnType=context.returnType;
    auto callerReturnJumps=std::move(context.returnJumps);
    const bool callerInInlineFunction=context.inInlineFunction;
    context.scopes.clear();
    context.scopes.emplace_back();
    context.returnType=signature.returnType;
    context.returnJumps.clear();
    context.inInlineFunction=true;
    context.expansionStack.push_back(call->text);

    std::vector<std::uint32_t> parameterSlots;
    bool setupOk=true;
    for(std::size_t i=0;i<signature.declaration->parameters.size();++i) {
        const auto& parameter=signature.declaration->parameters[i];
        if(context.bytecode.locals.size()>=65'536) {
            errors.push_back("QVM local limit exceeded while expanding function parameters");
            setupOk=false;
            break;
        }
        const auto slot=static_cast<std::uint32_t>(context.bytecode.locals.size());
        context.bytecode.locals.push_back("$taviq_"+call->text+"_"+parameter.name+"_"+std::to_string(slot));
        parameterSlots.push_back(slot);
        context.scopes.back().emplace(parameter.name,
            Binding{slot,signature.parameterTypes[i],false});
    }
    if(setupOk) {
        for(std::size_t i=temporarySlots.size();i>0;--i)
            emit(context,OpCode::LoadLocal,temporarySlots[i-1]);
        for(const auto slot:parameterSlots)
            emit(context,OpCode::StoreLocalMutable,slot);
    }

    bool bodyOk=setupOk;
    if(setupOk)bodyOk=compileStatements(signature.declaration->body,context,errors,false)&&bodyOk;
    const auto returnTarget=context.bytecode.code.size();
    for(const auto jump:context.returnJumps)
        context.bytecode.code[jump].slot=static_cast<std::uint32_t>(returnTarget);

    context.expansionStack.pop_back();
    context.scopes=std::move(callerScopes);
    context.returnType=callerReturnType;
    context.returnJumps=std::move(callerReturnJumps);
    context.inInlineFunction=callerInInlineFunction;
    return bodyOk;
}
}

bool compile(const Program& program,Bytecode& output,std::vector<std::string>& errors) {
    Bytecode result;
    result.formatVersion=2;
    Context context{result};

    const auto builtinName=[](const std::string& name) {
        return name=="len"||name=="sqrt"||name=="abs";
    };
    for(const auto& function:program.functions) {
        if(function.name.empty()||builtinName(function.name)||context.functions.contains(function.name)) {
            errors.push_back("duplicate, empty or reserved function name: "+function.name);
            continue;
        }
        FunctionSignature signature;
        signature.declaration=&function;
        if(!resolveTypeName(function.returnType,signature.returnType)) {
            errors.push_back("function '"+function.name+"' has an unknown return type: "+function.returnType);
            continue;
        }
        std::unordered_map<std::string,bool> parameterNames;
        bool valid=true;
        for(const auto& parameter:function.parameters) {
            Type type;
            if(parameter.name.empty()||parameterNames.contains(parameter.name)) {
                errors.push_back("function '"+function.name+"' has an empty or duplicate parameter name");
                valid=false;
                break;
            }
            if(!resolveTypeName(parameter.declaredType,type)) {
                errors.push_back("parameter '"+parameter.name+"' of function '"+function.name+
                    "' has an unknown type: "+parameter.declaredType);
                valid=false;
                break;
            }
            parameterNames.emplace(parameter.name,true);
            signature.parameterTypes.push_back(type);
        }
        if(valid)context.functions.emplace(function.name,std::move(signature));
    }
    if(!errors.empty())return false;

    /* Compile each definition once into a scratch module so even unused functions
       are type-checked and recursive call graphs are rejected. */
    Bytecode validationBytecode;
    Context validation{validationBytecode};
    validation.functions=context.functions;
    for(const auto& function:program.functions) {
        const auto signature=context.functions.find(function.name);
        if(signature==context.functions.end())return false;
        auto call=std::make_shared<Expr>();
        call->kind=Expr::Kind::Call;
        call->text=function.name;
        for(const auto type:signature->second.parameterTypes) {
            auto argument=std::make_shared<Expr>();
            if(type==Type::Number){argument->kind=Expr::Kind::Number;argument->text="0";}
            else if(type==Type::String){argument->kind=Expr::Kind::String;argument->text="";}
            else {argument->kind=Expr::Kind::Boolean;argument->boolean=false;}
            call->arguments.push_back(std::move(argument));
        }
        if(!emitInlineFunction(call,validation,errors))return false;
    }

    const bool ok=compileStatements(program.statements,context,errors,false);
    if(!ok||!errors.empty())return false;
    if(result.code.size()>=1'000'000) {
        errors.push_back("program exceeds QVM instruction limit");
        return false;
    }
    if(std::any_of(program.statements.begin(),program.statements.end(),
        [](const Statement& statement){return statement.kind==Statement::Kind::Return;})) {
        // Return outside a function is already diagnosed by compileStatements.
        return false;
    }
    emit(context,OpCode::Halt);
    std::string verifyError;
    if(!verifyBytecode(result,verifyError)) {
        errors.push_back("compiler emitted invalid QVM: "+verifyError);
        return false;
    }
    output=std::move(result);
    return true;
}
}
