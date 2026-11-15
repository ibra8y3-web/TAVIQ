#include <jni.h>
#include "taviq/compiler.hpp"
#include "taviq/lexer.hpp"
#include "taviq/parser.hpp"
#include "taviq/vm.hpp"
#include <sstream>
#include <string>
#include <vector>

extern "C" JNIEXPORT jstring JNICALL
Java_org_taviq_app_NativeRuntime_run(JNIEnv* env,jclass,jstring source) {
    if (!source) return env->NewStringUTF("Error: source is null");
    const char* utf=env->GetStringUTFChars(source,nullptr);
    if (!utf) return env->NewStringUTF("Error: unable to read source text");
    std::string code(utf);
    env->ReleaseStringUTFChars(source,utf);
    if (code.size()>1024*1024) return env->NewStringUTF("Error: source exceeds 1 MiB");

    std::ostringstream output;
    try {
        taviq::Lexer lexer(code);
        const auto tokens=lexer.scan();
        if (!lexer.errors().empty()) {
            for (const auto& error:lexer.errors()) output<<error<<'\n';
        } else {
            taviq::Parser parser(tokens);
            const auto program=parser.parse();
            if (!parser.errors().empty()) {
                for (const auto& error:parser.errors()) output<<error<<'\n';
            } else {
                taviq::Bytecode bytecode;
                std::vector<std::string> errors;
                if (!taviq::compile(program,bytecode,errors)) {
                    for (const auto& error:errors) output<<error<<'\n';
                } else {
                    std::string runtimeError;
                    if (taviq::VM{}.run(bytecode,output,runtimeError)!=0)
                        output<<"Runtime error: "<<runtimeError<<'\n';
                }
            }
        }
    } catch (const std::exception& ex) {
        output<<"Native exception: "<<ex.what()<<'\n';
    } catch (...) {
        output<<"Native exception: unknown failure\n";
    }
    const std::string result=output.str();
    return env->NewStringUTF(result.c_str());
}
