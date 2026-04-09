#include "ASTPrintVisitor.hpp"
#include <iostream>

namespace AST {
//------------------------------------------------------------------------
// 注意：调用 print_prefix 的时候要先 push 再调用，调用完再 pop
void ASTPrintVisitor::print_prefix(bool is_last) const {
    // 根据 is_last_list 生成当前节点的缩进字符串
    for (int i = 0; i < (int)is_last_list.size(); ++i) {
        if (i == (int)is_last_list.size() - 1) break;
        std::cout << (is_last_list[i] ? SPACE : PIPE);
    }

    if (!is_last_list.empty())
        std::cout << (is_last ? L_BRANCH : BRANCH);
}
//------------------------------------------------------------------------
void ASTPrintVisitor::printAST(const std::shared_ptr<CompUnitAST>& root) {
    std::cout << "ASTDump:\n";
    root->accept(*this);
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(CompUnitAST &node) {
    print_prefix(true);
    std::cout << "CompUnit\n";
    const auto& ch = node.getNodes();
    for (size_t i = 0; i < ch.size(); ++i) {
        is_last_list.push_back(i == ch.size()-1);
        ch[i]->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(DeclStmtAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "DeclStmt<const:" << (node.isConst()?"true":"false")
              << ", type:" << typeToString(node.getType()) << ">\n";
    const auto& ch = node.getVarDefs();
    for (size_t i = 0; i < ch.size(); ++i) {
        is_last_list.push_back(i == ch.size()-1);
        ch[i]->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(VarDefAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "VarDef<const:" << (node.isConst()?"true":"false")
              << ", type:" << typeToString(node.getType())
              << ", name:" << node.getName()
              << ", array:" << (node.isArray()?"true":"false")
              << ", init:" << (node.isInited()?"true":"false") << ">\n";
    if (node.isArray()) {
        const auto& subs = node.getSubscripts();
        for (size_t i = 0; i < subs.size(); ++i) {
            // is_last_list.push_back(i == subs.size()-1 && !node.isInited()); // 如果没有初始化值且没有下一个变量声明，那么数组维度列表的最后一个维度就是整个变量声明的最后一个子节点
            // print_prefix(is_last_list.back());
            // std::cout << "ArraySubscript\n";
            is_last_list.push_back(i == subs.size()-1 && !node.isInited()); // 如果没有初始化值且没有下一个变量声明，那么数组维度列表的最后一个维度就是整个变量声明的最后一个子节点
            subs[i]->accept(*this);
            is_last_list.pop_back();
            // is_last_list.pop_back();
        }
    }
    if (node.isInited()) {
        is_last_list.push_back(true);
        // print_prefix(true);
        // std::cout << "Init\n";
        // is_last_list.push_back(true);
        node.getInitVal()->accept(*this);
        // is_last_list.pop_back();
        is_last_list.pop_back();
    }
    // if (node.next) {
    //     is_last_list.push_back(true);
    //     print_prefix(true);
    //     std::cout << "NextVarDecl\n";
    //     is_last_list.push_back(true);
    //     node.next->accept(*this);
    //     is_last_list.pop_back();
    //     is_last_list.pop_back();
    // }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(InitValAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "InitVal<list:" << (node.isList()?"true":"false")
              << ", empty:" << (node.isEmptyList()?"true":"false") << ">\n";
    if (!node.isList() && node.getExp()) {
        is_last_list.push_back(true);
        node.getExp()->accept(*this);
        is_last_list.pop_back();
    }
    if (node.isList() && !node.isEmptyList()) {
        const auto& ch = node.getInner();
        for (size_t i = 0; i < ch.size(); ++i) {
            is_last_list.push_back(i == ch.size()-1);
            ch[i]->accept(*this);
            is_last_list.pop_back();
        }
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(ArraySubscriptAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "ArraySubscript\n";
    if (node.getExp()) {
        is_last_list.push_back(true);
        node.getExp()->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(FuncDefAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "FuncDef<returnType:" << typeToString(node.getType())
              << ", name:" << node.getName() << ">\n";
    if (!node.isEmptyParam()) {
        const auto& ps = node.getParams();
        is_last_list.push_back(!ps.empty() && !node.getBody()); // 如果参数列表不为空且没有函数体，那么函数定义的最后一个子节点就是参数列表（虽然它是空的）
        print_prefix(is_last_list.back());
        std::cout << "Params\n";
        for (size_t i = 0; i < ps.size(); ++i) {
            is_last_list.push_back(i == ps.size()-1); // 如果没有函数体，参数列表的最后一个参数就是整个函数定义的最后一个子节点
            ps[i]->accept(*this);
            is_last_list.pop_back();
        }
        is_last_list.pop_back();
    }
    if (node.getBody()) {
        is_last_list.push_back(true);
        print_prefix(is_last_list.back());
        std::cout << "Body\n";
        is_last_list.push_back(true);
        node.getBody()->accept(*this);
        is_last_list.pop_back();
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(FuncFParamAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "FuncFParam<type:" << typeToString(node.getType())
              << ", name:" << node.getName()
              << ", array:" << (node.isArray()?"true":"false") << ">\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(DeclRefAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "DeclRef<name:" << node.getName() << ">\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(ArrayExprAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "ArrayExpr<name:" << node.getName() << ">\n";
    const auto& idx = node.getIndices();
    for (size_t i = 0; i < idx.size(); ++i) {
        is_last_list.push_back(i == idx.size()-1);
        idx[i]->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(CallExprAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "CallExpr<callee:" << node.getName() << ">\n";
    if (!node.isEmptyParam()) {
        const auto& args = node.getParams();
        for (size_t i = 0; i < args.size(); ++i) {
            is_last_list.push_back(i == args.size()-1);
            args[i]->accept(*this);
            is_last_list.pop_back();
        }
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(FuncRParamAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "FuncRParam\n";
    if (node.getExp()) {
        is_last_list.push_back(true);
        node.getExp()->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(BinaryExprAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "BinaryExpr<op:" << binaryOpToString(node.getBinaryOpType()) << ">\n";
    is_last_list.push_back(false);
    node.getLhs()->accept(*this);
    is_last_list.pop_back();
    is_last_list.push_back(true);
    node.getRhs()->accept(*this);
    is_last_list.pop_back();
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(UnaryExprAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "UnaryExpr<op:" << unaryOpToString(node.getUnaryOpType()) << ">\n";
    is_last_list.push_back(true);
    node.getExp()->accept(*this);
    is_last_list.pop_back();
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(IntLiteralAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "IntLiteral<value:" << node.getValue() << ">\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(FloatLiteralAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "FloatLiteral<value:" << node.getValue() << ">\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(BlockStmtAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "BlockStmt\n";
    if (!node.isEmpty()) {
        const auto& stmts = node.getItems();
        for (size_t i = 0; i < stmts.size(); ++i) {
            is_last_list.push_back(i == stmts.size()-1);
            stmts[i]->accept(*this);
            is_last_list.pop_back();
        }
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(IfStmtAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "IfStmt<hasElse:" << (node.hasElse()?"true":"false") << ">\n";
    is_last_list.push_back(false);
    print_prefix(is_last_list.back());
    std::cout << "Cond\n";
    is_last_list.push_back(true);
    node.getCond()->accept(*this);
    is_last_list.pop_back();
    is_last_list.pop_back();
    
    is_last_list.push_back(!node.hasElse());
    print_prefix(is_last_list.back());
    std::cout << "Then\n";
    is_last_list.push_back(true);
    node.getThenBody()->accept(*this);
    is_last_list.pop_back();
    is_last_list.pop_back();
    if (node.hasElse()) {
        is_last_list.push_back(true);
        print_prefix(is_last_list.back());
        std::cout << "Else\n";
        
        is_last_list.push_back(true);
        node.getElseBody()->accept(*this);
        is_last_list.pop_back();
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(WhileStmtAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "WhileStmt\n";
    is_last_list.push_back(false);
    print_prefix(is_last_list.back());
    std::cout << "Cond\n";
    is_last_list.push_back(true);
    node.getCondition()->accept(*this);
    is_last_list.pop_back();
    is_last_list.pop_back();
    is_last_list.push_back(true);
    print_prefix(is_last_list.back());
    std::cout << "Body\n";
    is_last_list.push_back(true);
    node.getBody()->accept(*this);
    is_last_list.pop_back();
    is_last_list.pop_back();
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(ReturnStmtAST &node) {
    print_prefix(is_last_list.back());
    std::cout << "ReturnStmt\n";
    if (node.hasReturnExpr()) {
        is_last_list.push_back(true);
        node.getReturnExpr()->accept(*this);
        is_last_list.pop_back();
    }
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(BreakStmtAST &node) {
    print_prefix(is_last_list.back()); std::cout << "BreakStmt\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(ContinueStmtAST &node) {
    print_prefix(is_last_list.back()); std::cout << "ContinueStmt\n";
}
//------------------------------------------------------------------------
void ASTPrintVisitor::visit(NullStmtAST &node) {
    print_prefix(is_last_list.back()); std::cout << "NullStmt\n";
}
//------------------------------------------------------------------------
} // namespace AST