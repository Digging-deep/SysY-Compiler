#ifndef SYSY2022_SEMA_CHECK_VISITOR_HPP
#define SYSY2022_SEMA_CHECK_VISITOR_HPP

#include "AST.hpp"
#include "SCSymTable.hpp"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

namespace Sema {
    
class SemaCheckVisitor : public AST::ASTVisitor {
private:
    SCSymTable symbolTable;
    int loopDepth = 0;
    AST::BType currentFuncReturnType = AST::BType::UNDEFINED;
    std::string currentFuncName;
    bool hasError = false;

    void error(const yy::location& loc, const std::string& msg);
    
    // 因为表达式的结果有可能是数组类型，所以这里返回的是EType而不是BType
    // EType getExprType(ExprAST* expr);

    // 判断表达式是否为编译期常量
    // 注意：该函数通过是否成功计算编译期常量来判断表达式是否是编译期常量
    // 不能光通过AST节点来判断是否是编译期常量
    // 因为 2/0、2%0 这种表达式两端的操作数虽然是编译期常量，但是该表达式不是合法的编译期常量
    // 因为一个是 division by zero 错误，一个是 remainder by zero 错误
    bool isCompileTimeConstant(AST::ExprAST* expr);
    
    // 计算编译期常量的值
    // 返回值：如果表达式是编译期常量，返回其值（int 或 float）
    // 如果表达式不是编译期常量，返回 std::monostate{}
    std::variant<std::monostate, int, float> evaluateCompileTimeConstant(AST::ExprAST* expr);
    
    bool isIntType(AST::BType t) const { return t == AST::BType::INT; }
    bool isFloatType(AST::BType t) const { return t == AST::BType::FLOAT; }
    bool isNumericType(AST::BType t) const { return t == AST::BType::INT || t == AST::BType::FLOAT; }

    // 检查InitValAST中的单个初值或数组初始化器中的数值是否全为编译期常量
    // 注意：InitValAST经过语义分析后，才能使用该函数
    bool checkInitValListConst(AST::InitValAST* initVal);

    // 检查InitValAST中的单个初值或数组初始化器中的数值是否全为指定类型
    // 注意：数组元素初值类型应与数组元素声明类型一致，例如整型数组初值列表中不能出现浮点型元素；但是浮点型数组的初始化列表中可以出现整型常量或整型常量表达式
    // 注意：InitValAST经过语义分析后，才能使用该函数
    bool checkInitValListType(AST::InitValAST* initVal, AST::BType targetType);

    // 检查数组初始化器是否与数组维度匹配
    // 入口函数 → 做准备、检查、调用递归函数checkListrec
    // 注意：checkList只检查数组初始化器是否与数组维度匹配（结构性检查），不检查数组初始化器中的数值是否全为编译期常量以及类型是否匹配
    bool checkList(const std::vector<int>& dimens, AST::InitValAST* initVal);

    // 参数：
    // 1. const std::vector<int>& dimens: 数组维度
    // 2. InitValAST* initVal: 数组初始化器，其中 initVal->isList() 必须为true
    // 3. int num: 已填充的数组元素（并没有真的填充，只是统一一下数量，真正的求值填充放在IR Generation去做）
    // 4. int depth: 当前嵌套列表深度，从0开始，0表示最外层列表
    // 返回值：如果数组初始化器与数组维度匹配，返回 true；否则返回 false
    bool checkListrec(const std::vector<int>& dimens, AST::InitValAST* initVal, int& num, int depth);

    // 入口函数 → 做准备、检查、调用递归函数flattenListrec
    // 注意：该函数需要在checkInitValListConst返回true后才能调用，即需要先确保数组初始化器中的所有元素都是编译期常量
    // 语义阶段只能扁平化常量数组初始化器，而非常量数组初始化器只能在IR Generation阶段扁平化，因为你没法在语义阶段就算出其中的非编译期常量值
    // flattenConstList不仅包含了checkList的功能，还具备扁平化的功能
    // 如果数组初始化器与数组维度匹配，返回 true；否则返回 false
    bool flattenConstList(const std::vector<int>& dimens, AST::InitValAST* initVal, std::vector<float>& resultList);
    // 递归地将常量数组初始化器展开为一维数组
    bool flattenConstListrec(const std::vector<int>& dimens, AST::InitValAST* initVal, int& num, int depth, std::vector<float>& resultList);

    void addBuiltinFunctions(); // 未完成


public:
    SemaCheckVisitor();
    
    bool check(const std::shared_ptr<AST::CompUnitAST>& root);
    bool hasErrors() const { return hasError; }

    void visit(AST::CompUnitAST& node) override; // 完成
    void visit(AST::DeclStmtAST& node) override; // 完成
    void visit(AST::VarDefAST& node) override;
    void visit(AST::InitValAST& node) override; // 完成
    void visit(AST::ArraySubscriptAST& node) override; // 完成
    void visit(AST::FuncDefAST& node) override; // 完成
    void visit(AST::FuncFParamAST& node) override; // 完成
    void visit(AST::DeclRefAST& node) override; // 完成
    void visit(AST::ArrayExprAST& node) override; // 完成
    void visit(AST::CallExprAST& node) override;  // 完成
    void visit(AST::FuncRParamAST& node) override; // 完成
    void visit(AST::BinaryExprAST& node) override; // 完成
    void visit(AST::UnaryExprAST& node) override; // 完成
    void visit(AST::IntLiteralAST& node) override; // 完成
    void visit(AST::FloatLiteralAST& node) override; // 完成
    void visit(AST::BlockStmtAST& node) override; // 完成
    void visit(AST::IfStmtAST& node) override;  // 完成
    void visit(AST::WhileStmtAST& node) override; // 完成
    void visit(AST::ReturnStmtAST& node) override; // 完成
    void visit(AST::BreakStmtAST& node) override; // 完成
    void visit(AST::ContinueStmtAST& node) override; // 完成
    void visit(AST::NullStmtAST& node) override; // 完成
};

} // namespace Sema

#endif 
