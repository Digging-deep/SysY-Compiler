#ifndef SYSY2022_IRGEN_VISITOR_HPP
#define SYSY2022_IRGEN_VISITOR_HPP

#include "AST.hpp"
#include "IRGSymTable.hpp"
#include "SemaCheckVisitor.hpp"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Verifier.h"
#include <memory>
#include <vector>
#include <string>
#include <stack>

#include "DebugInfoGenerator.hpp"

namespace IRGen {

class IRGenVisitor : public AST::ASTVisitor {
private:
    std::unique_ptr<llvm::LLVMContext>& context;
    std::unique_ptr<llvm::Module>& module;
    std::unique_ptr<llvm::IRBuilder<>>& builder;
    Sema::SemaCheckVisitor& semaCheck;
    llvm::Type* voidType;
    llvm::Type* intType;
    llvm::Type* floatType;
    llvm::Type* ptrType;
    llvm::Constant* zeroInt;
    llvm::Constant* zeroFloat;
    
    std::unique_ptr<AST::DebugInfoGenerator> debugInfoGenerator;
    
    IRGSymTable symbolTable;
    
    AST::BType currentFuncReturnType;
    
    struct LoopInfo {
        llvm::BasicBlock* condBlock;
        llvm::BasicBlock* endBlock;
    };
    std::stack<LoopInfo> loopStack;
    
    llvm::Value* currentValue;
    
    llvm::BasicBlock* allocaInsertBlock;
    llvm::BasicBlock::iterator allocaInsertPoint;
    
    llvm::Type* getLLVMType(AST::BType type);
    llvm::Type* getLLVMArrayType(llvm::Type* baseType, const std::vector<int>& dims);
    
    llvm::Value* createCast(llvm::Value* value, llvm::Type* toType);
    llvm::Value* convertToBool(llvm::Value* value);
    llvm::Value* convertToInt(llvm::Value* value);
    llvm::Value* convertToFloat(llvm::Value* value);
    
    llvm::AllocaInst* createAlloca(llvm::Type* type, llvm::Value* arraySize);

    // 扁平化数组初始化器，不管是不是编译期常量初始化器，都能处理
    void flattenList(llvm::Type* baseType, const std::vector<int>& dimens, AST::InitValAST* initVal, std::vector<llvm::Value*>& resultList);
    // 递归地将数组初始化器展开为一维数组
    //（该函数其实有点过度检查，因为有些检查在语义分析阶段就做过了，所以有些重复检查了）
    void flattenListrec(llvm::Type* baseType, const std::vector<int>& dimens, AST::InitValAST* initVal, int& num, int depth, std::vector<llvm::Value*>& resultList);

    void addBuiltinFunctions();

public:
    IRGenVisitor(std::unique_ptr<llvm::LLVMContext>& context,
                                std::unique_ptr<llvm::Module>& module,
                                std::unique_ptr<llvm::IRBuilder<>>& builder,
                                Sema::SemaCheckVisitor& semaCheck);
    ~IRGenVisitor();
    
    // 设置是否启用调试信息
    void setEnableDebugInfo(bool enable);
    
    bool generate(const std::shared_ptr<AST::CompUnitAST>& root);
    
    void printIR();
    void printIR(std::ostream& os);

    void saveIRToFile(const std::string& filename);

    llvm::Module* getModule() { return module.get(); }

    void visit(AST::CompUnitAST& node) override;
    void visit(AST::DeclStmtAST& node) override;
    void visit(AST::VarDefAST& node) override;
    void visit(AST::InitValAST& node) override;
    void visit(AST::ArraySubscriptAST& node) override;
    void visit(AST::FuncDefAST& node) override;
    void visit(AST::FuncFParamAST& node) override;
    void visit(AST::DeclRefAST& node) override;
    void visit(AST::ArrayExprAST& node) override;
    void visit(AST::CallExprAST& node) override;
    void visit(AST::FuncRParamAST& node) override;
    void visit(AST::BinaryExprAST& node) override;
    void visit(AST::UnaryExprAST& node) override;
    void visit(AST::IntLiteralAST& node) override;
    void visit(AST::FloatLiteralAST& node) override;
    void visit(AST::BlockStmtAST& node) override;
    void visit(AST::IfStmtAST& node) override;
    void visit(AST::WhileStmtAST& node) override;
    void visit(AST::ReturnStmtAST& node) override;
    void visit(AST::BreakStmtAST& node) override;
    void visit(AST::ContinueStmtAST& node) override;
    void visit(AST::NullStmtAST& node) override;
};

}  // namespace IRGen

#endif  // SYSY2022_IRGEN_VISITOR_HPP
