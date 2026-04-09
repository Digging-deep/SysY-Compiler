#include <iostream>
#include <memory>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Support/TargetSelect.h" // 包含所有目标初始化函数
#include "llvm/TargetParser/Triple.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "lexer.hpp"
#include "parser.tab.hpp"
#include "YYInGuard.hpp" // 利用RAII管理yyin
#include "ASTPrintVisitor.hpp"
#include "SemaCheckVisitor.hpp"
#include "IRGenVisitor.hpp"


/*************************************************************************************************************/
extern std::shared_ptr<AST::CompUnitAST> astTree;

enum OptLevel {
    O0, O1, O2, O3
};
/*************************************************************************************************************/
// 可以在生成LLVM IR之后单独使用mem2reg
// 但是如果启用了优化的话，就没必要单独使用mem2reg
// 因为 O1、O2、O3 都会自动执行(但 O0 不会运行 mem2reg！)
void runMem2Reg(llvm::Module* module) {
    // 创建各种 AnalysisManager
    llvm::PassBuilder pb;
    llvm::FunctionAnalysisManager fam;
    llvm::ModuleAnalysisManager mam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::LoopAnalysisManager lam;

    // 交叉注册
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    // 创建 FunctionPassManager 并添加 mem2reg
    llvm::FunctionPassManager fpm;
    fpm.addPass(llvm::PromotePass());

    // 对 module 中每个函数运行
    for (auto& func : *module) {
        if (!func.isDeclaration()) {  // 跳过函数声明，只处理有函数体的函数
            fpm.run(func, fam);
        }
    }
}
/*************************************************************************************************************/
void runOptimization(llvm::Module* module, OptLevel optLevel) {
    llvm::PassBuilder pb;
    llvm::FunctionAnalysisManager fam;
    llvm::ModuleAnalysisManager mam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::LoopAnalysisManager lam;

    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);

    // 根据优化级别选择
    llvm::OptimizationLevel level;
    switch (optLevel) {
        case O0: level = llvm::OptimizationLevel::O0; break;
        case O1: level = llvm::OptimizationLevel::O1; break;
        case O2: level = llvm::OptimizationLevel::O2; break;
        case O3: level = llvm::OptimizationLevel::O3; break;
        default: level = llvm::OptimizationLevel::O0; break;
    }

    llvm::ModulePassManager mpm;
    if (optLevel == O0) {
        mpm = pb.buildO0DefaultPipeline(level);
    } else {
        mpm = pb.buildPerModuleDefaultPipeline(level);
    }

    mpm.run(*module, mam);
}
/*************************************************************************************************************/
void generateRISCVAssembly(llvm::Module* module, const std::string& outputFilename) {
    // 初始化 RISC-V 目标
    LLVMInitializeRISCVTargetInfo();
    LLVMInitializeRISCVTarget();
    LLVMInitializeRISCVTargetMC();
    LLVMInitializeRISCVAsmPrinter();

    // #include "llvm/Support/TargetSelect.h"
    // 如果想初始化所有目标，这按照下面这样初始化
    // llvm::InitializeAllTargets();
    // llvm::InitializeAllTargetMCs(); 
    // llvm::InitializeAllAsmPrinters();
    // llvm::InitializeAllAsmParsers();

    // 设置目标 triple
    std::string targetTriple = "riscv64-unknown-linux-gnu";
    module->setTargetTriple(targetTriple);

    // 查找目标
    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        llvm::errs() << "Target not found: " << error << "\n";
        return;
    }

    // 创建 TargetMachine
    llvm::TargetOptions opt;
    auto rm = std::optional<llvm::Reloc::Model>();
    // RISC-V针对64位处理器的最小指令集是RV64I，表示64位基础整型指令集
    // +m,+a,+f,+d 是在 I 的基础上叠加扩展，而不是替换。I 是 RV64 的地基，永远隐含在内
    // 你无法创建一个"没有 I"的 RV64 目标机器。
    // 这段代码对应的实际 ISA 是 RV64IMAFD，也就是 RV64G（G 是 IMAFD 的别名）。
    llvm::TargetMachine* targetMachine = target->createTargetMachine(
        targetTriple,
        "generic-rv64",       // CPU
        "+m,+a,+f,+d,+c",  // 特性：乘除法、原子、单精度浮点、双精度浮点、压缩指令
        opt,
        rm
    );

    // 设置 DataLayout
    module->setDataLayout(targetMachine->createDataLayout());

    // 打开输出文件
    std::error_code ec;
    llvm::raw_fd_ostream dest(outputFilename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        llvm::errs() << "Could not open file: " << ec.message() << "\n";
        return;
    }

    // 生成汇编代码
    llvm::legacy::PassManager pm;
    auto FileType = llvm::CodeGenFileType::AssemblyFile; // 如果生成汇编的话，请改为 llvm::CodeGenFileType::ObjectFile
    if (targetMachine->addPassesToEmitFile(pm, dest, nullptr, FileType)) {
        llvm::errs() << "Target machine cannot emit assembly file\n";
        return;
    }

    pm.run(*module);
    dest.flush();

    // llvm::outs() << "Assembly saved to " << outputFilename << "\n";
}
/*************************************************************************************************************/
// 自定义版本打印函数（核心）
void printCustomVersion(llvm::raw_ostream &OS) {
    // 1. 编译器基础信息
    OS << "====================================\n";
    OS << "          SysY2022 Compiler         \n";
    OS << "====================================\n";
    OS << "Author: Mingze Wu\n";
    OS << "Target: SysY2022 Language -> LLVM IR -> Machine Code\n";
    OS << "Supported Architectures: x86_64, RISCV64, ARM64\n";
    // 2. 依赖的LLVM版本（通过LLVM内置函数获取）
    OS << "LLVM Version: " << LLVM_VERSION_STRING << "\n";
    // 3. 编译时间（可选，需提前定义）
    OS << "Build Time: " << __DATE__ << " " << __TIME__ << "\n";
    OS << "====================================\n";
}
/*************************************************************************************************************/
static const char *Head = "SysY2022 - SysY2022 compiler";

static llvm::cl::opt<std::string>
    inputFilename(llvm::cl::Positional,
                llvm::cl::desc("<input-files>"),
                llvm::cl::Required);
        
static llvm::cl::opt<std::string>
    outputFilename("o",
                    llvm::cl::desc("Output filename"),
                    llvm::cl::value_desc("filename"),
                    llvm::cl::init("output.s"));

static llvm::cl::opt<bool>
    debugMode(
        "debug",         
        llvm::cl::desc("Enable debug mode (disable optimizations, preserve debug info)."),
        llvm::cl::init(false),
        llvm::cl::Optional);

static llvm::cl::opt<bool>
    printAST(
        "print-ast",              
        llvm::cl::desc("Print the visualized abstract syntax tree."),
        llvm::cl::init(false),
        llvm::cl::Optional);

static llvm::cl::opt<bool>
    printIR(
        "print-ir",              
        llvm::cl::desc("Print LLVM IR to stdout."),
        llvm::cl::init(false),
        llvm::cl::Optional);


static llvm::cl::opt<OptLevel> optimizationLevel(llvm::cl::desc("Choose optimization level:"),
  llvm::cl::values(
    clEnumVal(O0, "Enable no optimizations"),
    clEnumVal(O1, "Enable basic optimizations"),
    clEnumVal(O2, "Enable full optimizations"),
    clEnumVal(O3, "Enable aggressive optimizations")),
    llvm::cl::init(O0));

/*************************************************************************************************************/
int main(int argc, char** argv)
{
    // 初始化LLVM环境
    // #include "llvm/Support/InitLLVM.h"
    llvm::InitLLVM init(argc, argv, Head);
    
    // 解析命令行参数
    // #include "llvm/Support/CommandLine.h"
    llvm::cl::SetVersionPrinter(printCustomVersion);
    llvm::cl::ParseCommandLineOptions(argc, argv, Head);

    // 创建LLVM上下文和模块
    std::unique_ptr<llvm::LLVMContext> context = std::make_unique<llvm::LLVMContext>();
    std::unique_ptr<llvm::Module> module = std::make_unique<llvm::Module>(inputFilename, *context);
    std::unique_ptr<llvm::IRBuilder<>> builder = std::make_unique<llvm::IRBuilder<>>(*context);

    // 设置模块的唯一标识符（Module Identifier）和模块对应的原始源文件名
    // module->setModuleIdentifier(inputFilename);
    // module->setSourceFileName(inputFilename);

    /*-------------------1. Lexical Analysis & Syntax Analysis---------------*/
    // 利用RAII管理flex中的yyin
    // 避免fopen后忘记fclose
    YYInGuard yyinGuard(inputFilename, "r");

    yy::parser parser;
    // parser.set_debug_level (1);
    parser.parse();

    /*-------------------2. AST----------------------------------------------*/
    if(astTree == nullptr) {
        throw std::logic_error("AST is empty!");
    }

    if(printAST) {
        AST::ASTPrintVisitor printer;
        printer.printAST(astTree);
    }

    /*-------------------2. Semantic Analysis--------------------------------*/
    Sema::SemaCheckVisitor checker;
    // if (checker.check(astTree)) {
    //     llvm::outs() << "Semantic check passed!" << "\n";
    // } else {
    //     llvm::errs() << "Semantic check fail!" << "\n";
    //     return 1;
    // }
    if(!checker.check(astTree)) {
        exit(1);
    }

    /*-------------------3. IR Generation------------------------------------*/
    IRGen::IRGenVisitor irGen{context, module, builder, checker};
    irGen.generate(astTree);
    // llvm::outs() << "Code generation completed." << "\n";

    // 可以单独进行mem2reg优化
    // 把 “栈上的内存变量” 提升成 “寄存器变量”，去掉 load/store，让代码更快
    // runMem2Reg(module.get());

    /*-------------------4. Optimization-------------------------------------*/
    runOptimization(module.get(), optimizationLevel);

    if(printIR)
        irGen.printIR();
    // irGen.saveIRToFile("a.out");

    /*-------------------5. Target Code Generation---------------------------*/
    generateRISCVAssembly(module.get(), outputFilename);

    return 0;
}

/**********************************************************************************/
 /**
    // 测试参数选项
    if(inputFilename.empty())
        std::cout << "No input file" << std::endl;
    else
        std::cout << "Input file: " << inputFilename << std::endl;

    if(outputFilename.empty())
        std::cout << "No output file" << std::endl;
    else
        std::cout << "Output file: " << outputFilename << std::endl;

    if(debugMode)
        std::cout << "Debug mode" << std::endl;
    else
        std::cout << "No debug mode" << std::endl;

    if(printAST)
        std::cout << "Print the AST" << std::endl;
    else
        std::cout << "Don't print the AST" << std::endl;

    if(optimizationLevel == O0)
        std::cout << "optimizationLevel: O0" << std::endl;
    else if(optimizationLevel == O1)
        std::cout << "optimizationLevel: O1" << std::endl;
    else if(optimizationLevel == O2)
        std::cout << "optimizationLevel: O2" << std::endl;
    else if(optimizationLevel == O3)
        std::cout << "optimizationLevel: O3" << std::endl;
    */
