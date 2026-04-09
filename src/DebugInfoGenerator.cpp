#include "DebugInfoGenerator.hpp"
#include "AST.hpp"

namespace AST {

DebugInfoGenerator::DebugInfoGenerator(llvm::LLVMContext& ctx, llvm::Module& mod, llvm::IRBuilder<>& bldr)
    : context(ctx), module(mod), builder(bldr) {
    diBuilder = std::make_unique<llvm::DIBuilder>(module);
    diCompileUnit = nullptr;
    diFile = nullptr;
    currentSubprogram = nullptr;
    enableDebugInfo = false;
}

DebugInfoGenerator::~DebugInfoGenerator() {
    if (diBuilder) {
        diBuilder->finalize();
    }
}

void DebugInfoGenerator::setEnableDebugInfo(bool enable) {
    enableDebugInfo = enable;
}

bool DebugInfoGenerator::isDebugInfoEnabled() const {
    return enableDebugInfo;
}

void DebugInfoGenerator::initialize() {
    if (!enableDebugInfo) {
        return;
    }
    
    // 初始化调试信息
    diFile = diBuilder->createFile("input.sy", ".");
    
    diCompileUnit = diBuilder->createCompileUnit(
        llvm::dwarf::DW_LANG_C,
        diFile,
        "SysY Compiler",
        false,
        "",
        0
    );
    
    // 设置调试信息版本
    module.addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
}

void DebugInfoGenerator::finalize() {
    if (!enableDebugInfo) {
        return;
    }
    
    if (diBuilder) {
        diBuilder->finalize();
    }
}

llvm::DISubprogram* DebugInfoGenerator::createFunctionDebugInfo(
    const std::string& funcName,
    llvm::FunctionType* funcType,
    llvm::Function* function,
    const std::vector<BType>& paramTypes,
    BType returnType
) {
    if (!enableDebugInfo) {
        return nullptr;
    }
    
    // 添加函数调试信息
    std::vector<llvm::Metadata*> diRetAndParamTypes;

    // 创建返回类型
    llvm::DIType* diReturnType = diBuilder->createBasicType(
        returnType == BType::INT ? "int" : (returnType == BType::FLOAT ? "float" : "void"),
        32,
        returnType == BType::INT ? llvm::dwarf::DW_ATE_signed : 
        (returnType == BType::FLOAT ? llvm::dwarf::DW_ATE_float : llvm::dwarf::DW_ATE_unsigned)
    );
    diRetAndParamTypes.push_back(diReturnType);

    for (const auto& paramType : paramTypes) {
        llvm::DIType* diType = diBuilder->createBasicType(
            paramType == BType::INT ? "int" : "float",
            32,
            paramType == BType::INT ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_float
        );
        diRetAndParamTypes.push_back(diType);
    }
    
    // 创建子例程类型
    llvm::DISubroutineType* subroutineType = diBuilder->createSubroutineType(
        diBuilder->getOrCreateTypeArray(diRetAndParamTypes) // 返回DITypeArray
    );
    
    // 创建函数
    llvm::DISubprogram* subprogram = diBuilder->createFunction(
        diCompileUnit,
        funcName,
        funcName,
        diFile,
        0,  // 行号
        subroutineType,
        0,  // 作用域行号
        llvm::DINode::FlagPrototyped,
        llvm::DISubprogram::SPFlagDefinition  // 是函数定义
    );
    
    function->setSubprogram(subprogram);
    currentSubprogram = subprogram;
    
    return subprogram;
}

void DebugInfoGenerator::setCurrentSubprogram(llvm::DISubprogram* subprogram) {
    currentSubprogram = subprogram;
}

void DebugInfoGenerator::createVariableDebugInfo(
    const std::string& varName,
    BType varType,
    llvm::Value* alloca
) {
    if (!enableDebugInfo || !currentSubprogram) {
        return;
    }
    
    llvm::DIType* diVarType = diBuilder->createBasicType(
        varType == BType::INT ? "int" : "float",
        32,
        varType == BType::INT ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_float
    );
    
    llvm::DILocalVariable* localVar = diBuilder->createAutoVariable(
        diBuilder->createLexicalBlockFile(currentSubprogram, diFile),
        varName,
        diFile,
        0,  // 行号
        diVarType
    );
    
    diBuilder->insertDeclare(
        alloca,
        localVar,
        diBuilder->createExpression(),
        llvm::DILocation::get(context, 0, 0, currentSubprogram),
        builder.GetInsertBlock()
    );
}

void DebugInfoGenerator::setDebugLocation(unsigned line, unsigned column) {
    if (!enableDebugInfo || !currentSubprogram) {
        return;
    }
    
    builder.SetCurrentDebugLocation(llvm::DILocation::get(
        context,
        line,
        column,
        currentSubprogram
    ));
}

} 