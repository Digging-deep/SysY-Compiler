#ifndef SYSY2022_AST_PRINT_VISITOR_HPP
#define SYSY2022_AST_PRINT_VISITOR_HPP

#include "AST.hpp"

namespace AST {

class ASTPrintVisitor : public ASTVisitor {
private:
    int indentLevel = 0;
    std::vector<bool> is_last_list;       // 只记录“是否是最后一个兄弟”

    const std::string PIPE    = "│  ";
    const std::string SPACE   = "   ";
    const std::string BRANCH  = "├─ ";
    const std::string L_BRANCH= "└─ ";

    void print_prefix(bool is_last) const;

public:
    void printAST(const std::shared_ptr<CompUnitAST>& root);
    void visit(CompUnitAST &node) override;
    void visit(DeclStmtAST &node) override;
    void visit(VarDefAST &node) override;
    void visit(InitValAST &node) override;
    void visit(ArraySubscriptAST &node) override;
    void visit(FuncDefAST &node) override;
    void visit(FuncFParamAST &node) override;
    void visit(DeclRefAST &node) override;
    void visit(ArrayExprAST &node) override;
    void visit(CallExprAST &node) override;
    void visit(FuncRParamAST &node) override;
    void visit(BinaryExprAST &node) override;
    void visit(UnaryExprAST &node) override;
    void visit(IntLiteralAST &node) override;
    void visit(FloatLiteralAST &node) override;
    void visit(BlockStmtAST &node) override;
    void visit(IfStmtAST &node) override;
    void visit(WhileStmtAST &node) override;
    void visit(ReturnStmtAST &node) override;
    void visit(BreakStmtAST &node) override;
    void visit(ContinueStmtAST &node) override;
    void visit(NullStmtAST &node) override;
};

} // namespace AST

#endif // SYSY2022_AST_PRINT_VISITOR_HPP