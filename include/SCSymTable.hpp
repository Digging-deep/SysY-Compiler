#ifndef SYSY2022_SC_SYM_TABLE_HPP
#define SYSY2022_SC_SYM_TABLE_HPP

#include "AST.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <variant>

namespace Sema {

enum class SymbolKind {
    VARIABLE,
    FUNCTION,
    PARAMETER
};

struct ArrayInfo {
    bool isArray = false;
    
    // 标记dimensions是否有效
    // 数组定义和数组形参的维度下标（如果有）必须满足以下三个条件：
    // 1. 必须是编译期常量
    // 2. 必须是整数
    // 3. 必须大于0
    bool isValid = true; 

    // 数组的维度
    // 如果是数组定义，那么每一维都有下标，存在 dimensions 中
    // 如果是一维数组形参，比如void func(int a[])中的int a[]，则dimensions={0}
    // 如果是多维数组形参，比如void func(int a[][2][3])中的int a[][2][3]，则dimensions={0,2, 3}
    // (SysY2022 语言规范要求，数组形参的第一维下标必须为空)
    std::vector<int> dimensions;

    
};

struct SymbolInfo {
    bool isConst = false;
    SymbolKind kind;
    AST::BType type;
    std::string name;
    ArrayInfo arrayInfo;
    
    // 存储函数参数的信息，只有kind为FUNCTION时才会有意义
    std::vector<SymbolInfo> params;
    
    // 存储 const 常量的编译期常量值
    // 使用 std::variant<int, float> 来存储整数或浮点数的编译期常量值
    std::variant<std::monostate, int, float> constValue;
};

struct Scope {
    std::unordered_map<std::string, SymbolInfo> symbols;
};

class SCSymTable {
private:
    // 存储变量和参数的作用域栈
    std::vector<Scope> scopeStack;
    
    // 存储函数的全局作用域
    Scope functionScope;
    
    // FuncDefAST中创建了一次作用域，用来记录形参
    // 在函数体BlockStmtAST中又会创建一次作用域，用来记录局部变量
    // 但是，这两个作用域应该是一个整体，而不是两个独立的作用域，所以需要合并作用域
    //
    // 合并作用域标志
    // 当为true时，不创建新作用域，直接在当前作用域插入符号
    bool mergeScope = false;

public:
    SCSymTable();
    
    void enterScope();
    void exitScope();
    
    // 插入和查找变量或参数的符号信息
    bool insertVar(const std::string& name, const SymbolInfo& info);
    bool insertVarInGlobalScope(const std::string& name, const SymbolInfo& info);
    SymbolInfo* lookupVar(const std::string& name); // 在语义分析阶段，可能需要频繁修改符号表条目，所以返回SymbolInfo*而非const SymbolInfo*
    SymbolInfo* lookupVarInCurrentScope(const std::string& name);
    SymbolInfo* lookupVarInGlobalScope(const std::string& name);
    
    // 插入和查找函数的符号信息
    bool insertFunc(const std::string& name, const SymbolInfo& info);
    SymbolInfo* lookupFunc(const std::string& name);
    
    void setMergeScope(bool merge) { mergeScope = merge; }
    bool isMergeScope() const { return mergeScope; }
    
    // 判断当前作用域是否是全局作用域
    bool isGlobalScope() const { return scopeStack.size() == 1; }
};

} // namespace Sema

#endif // SYSY2022_SC_SYM_TABLE_HPP
