#ifndef SYSY2022_IRG_SYM_TABLE_HPP
#define SYSY2022_IRG_SYM_TABLE_HPP

#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace IRGen {

// 符号类型枚举
enum class SymbolKind {
    GLOBAL_VARIABLE, // 比如在全局作用域中定义int a = 1;
    GLOBAL_ARRAY, // 比如在全局作用域中定义int a[2][3];
    LOCAL_VARIABLE, // 比如在局部作用域中定义int a = 1;
    LOCAL_ARRAY, // 比如在局部作用域中定义int a[2][3];
    PARAMETER_VARIABLE, // 比如函数形参int a
    PARAMETER_PTR, // 比如函数形参int a[][2][3]
    FUNCTION // 函数
};

// 符号信息结构
struct SymbolInfo {
    // 1. int a             (GLOBAL_VARIABLE OR LOCAL_VARIABLE OR PARAMETER_VARIABLE)
    // address: ptr->i32
    // type: i32
    //
    // 2. int a[2][3]       (GLOBAL_ARRAY OR LOCAL_ARRAY)
    // address: ptr->[2×[3×i32]]
    // type: [2×[3×i32]]
    //
    // 3. int a[][2][3]     (PARAMETER_PTR)
    // address: ptr->ptr->[2×[3×i32]]
    // type: [2×[3×i32]]
    // 注意：这里type不是ptr->[2×[3×i32]]，因为llvm统一改用opaque pointer，a是指针参数，我们需要记录它指向的数组类型

    SymbolKind kind;
    llvm::Value* address;
    llvm::Type* type; 
    
    SymbolInfo( SymbolKind k, llvm::Type* t,llvm::Value* addr) : kind(k), type(t), address(addr) {}
};

// 函数信息结构
struct FunctionInfo {
    SymbolKind kind;
    llvm::Function* function;

    FunctionInfo(llvm::Function* f) : function(f), kind(SymbolKind::FUNCTION) {}
};

class IRGSymTable {
private:
    // llvm::Value*中记录的是变量的地址，从该地址可以读取到变量的值
    // 每个SymbolInfo中记录的是变量的地址和变量的类型
    // 因为在进行IR Generation的时候，普通数组和数组参数的生成逻辑有些区别，所以需要记录变量的类型，以便区分普通数组和数组参数
    std::vector<std::unordered_map<std::string, SymbolInfo>> namedValues;
    
    std::unordered_map<std::string, FunctionInfo> functions;

    // FuncDefAST中创建了一次作用域，用来记录形参
    // 在函数体BlockStmtAST中又会创建一次作用域，用来记录局部变量
    // 但是，这两个作用域应该是一个整体，而不是两个独立的作用域，所以需要合并作用域
    //
    // 合并作用域标志
    // 当为true时，不创建新作用域，直接在当前作用域插入符号
    bool mergeScope = false;

public:
    IRGSymTable();
    
    void enterScope();
    void exitScope();
    
    void setNamedValue(const std::string& name, SymbolInfo info);
    SymbolInfo* getNamedValue(const std::string& name);
    
    void setFunction(const std::string& name, FunctionInfo info);
    FunctionInfo* getFunction(const std::string& name);

    void setMergeScope(bool merge) { mergeScope = merge; }
    bool isMergeScope() const { return mergeScope; }
    
    bool hasFunction(const std::string& name) const;

    bool isGlobalScope() const;
};

} // namespace IRGen

#endif // SYSY2022_IRG_SYM_TABLE_HPP
