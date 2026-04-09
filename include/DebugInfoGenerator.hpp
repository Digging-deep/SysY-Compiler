#ifndef SYSY2022_DEBUG_INFO_GENERATOR_HPP
#define SYSY2022_DEBUG_INFO_GENERATOR_HPP

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/IRBuilder.h"
#include <memory>
#include <string>

#include "AST.hpp"

namespace AST {

class DebugInfoGenerator {
private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    
    std::unique_ptr<llvm::DIBuilder> diBuilder;
    
    // 调试信息相关
    llvm::DICompileUnit* diCompileUnit;
    llvm::DIFile* diFile;
    llvm::DISubprogram* currentSubprogram;
    
    // 是否启用调试信息
    bool enableDebugInfo;

public:
    DebugInfoGenerator(llvm::LLVMContext& ctx, llvm::Module& mod, llvm::IRBuilder<>& bldr);
    ~DebugInfoGenerator();
    
    // 设置是否启用调试信息
    void setEnableDebugInfo(bool enable);
    
    // 获取是否启用调试信息
    bool isDebugInfoEnabled() const;
    
    // 初始化调试信息
    void initialize();
    
    // 完成调试信息构建
    void finalize();
    
    // 为函数创建调试信息
    llvm::DISubprogram* createFunctionDebugInfo(
        const std::string& funcName,
        llvm::FunctionType* funcType,
        llvm::Function* function,
        const std::vector<BType>& paramTypes,
        BType returnType
    );
    
    // 设置当前函数调试信息
    void setCurrentSubprogram(llvm::DISubprogram* subprogram);
    
    // 为变量创建调试信息
    void createVariableDebugInfo(
        const std::string& varName,
        BType varType,
        llvm::Value* alloca
    );
    
    // 设置调试位置
    void setDebugLocation(unsigned line, unsigned column);
};

} 

#endif 