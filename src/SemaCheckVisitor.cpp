#include "SemaCheckVisitor.hpp"
#include "llvm/Support/Casting.h"
#include <climits> // just for INT_MIN
#include <cmath> // just for std::isinf
#include <iostream>

namespace Sema {
void SemaCheckVisitor::error(const yy::location& loc, const std::string& msg) {
    hasError = true;
    llvm::errs() << "error" << "(" << loc.begin.line << "," << loc.begin.column << "): ";
    llvm::errs() << "semantic error, " << msg << "\n";
}
/*******************************************************************************************/
std::variant<std::monostate, int, float> SemaCheckVisitor::evaluateCompileTimeConstant(AST::ExprAST* expr) {
    if (!expr) return std::monostate{};
    
    // 整数字面量：返回 int
    if (auto intLit = llvm::dyn_cast<AST::IntLiteralAST>(expr)) {
        return intLit->getValue();
    }
    
    // 浮点数字面量：返回 float
    if (auto floatLit = llvm::dyn_cast<AST::FloatLiteralAST>(expr)) {
        return floatLit->getValue();
    }
    
    // 变量引用：如果是 const 变量且有编译期常量值，则返回该值
    if (auto declRef = llvm::dyn_cast<AST::DeclRefAST>(expr)) {
        SymbolInfo* symbol = symbolTable.lookupVar(declRef->getName());
        if (symbol && symbol->isConst && !std::holds_alternative<std::monostate>(symbol->constValue)) {
            return symbol->constValue;
        }
        return std::monostate{};
    }
    
    // 数组元素访问：不是编译期常量
    if (llvm::isa<AST::ArrayExprAST>(expr)) {
        return std::monostate{};
    }
    
    // 函数调用：不是编译期常量
    if (llvm::isa<AST::CallExprAST>(expr)) {
        return std::monostate{};
    }
    
    // 函数实参：计算其内部表达式
    if (auto funcRParam = llvm::dyn_cast<AST::FuncRParamAST>(expr)) {
        return evaluateCompileTimeConstant(funcRParam->getExp().get());
    }
    
    // 一元表达式：根据操作符计算
    if (auto unaryExpr = llvm::dyn_cast<AST::UnaryExprAST>(expr)) {
        auto operandVal = evaluateCompileTimeConstant(unaryExpr->getExp().get());
        if (std::holds_alternative<std::monostate>(operandVal)) {
            return std::monostate{};
        }
        
        switch (unaryExpr->getUnaryOpType()) {
            case AST::UnaryOpType::PLUS:
                return operandVal;
            case AST::UnaryOpType::MINUS:
                if (std::holds_alternative<int>(operandVal)) {
                    return -std::get<int>(operandVal);
                } else {
                    return -std::get<float>(operandVal);
                }
            case AST::UnaryOpType::NOT:
                // 逻辑非：0返回1，非0返回0
                if (std::holds_alternative<int>(operandVal)) {
                    return std::get<int>(operandVal) == 0 ? 1 : 0;
                } else {
                    return std::get<float>(operandVal) == 0.0f ? 1 : 0;
                }
            default:
                return std::monostate{};
        }
    }
    
    // 二元表达式：根据操作符计算
    if (auto binaryExpr = llvm::dyn_cast<AST::BinaryExprAST>(expr)) {
        // 赋值表达式不是编译期常量
        if (binaryExpr->getBinaryOpType() == AST::BinaryOpType::ASSIGN) {
            return std::monostate{};
        }
        
        // 逻辑与和逻辑或需要短路求值
        if (binaryExpr->getBinaryOpType() == AST::BinaryOpType::AND) {
            auto lhsVal = evaluateCompileTimeConstant(binaryExpr->getLhs().get());
            if (std::holds_alternative<std::monostate>(lhsVal)) return std::monostate{};
            bool lhsIsZero = std::holds_alternative<int>(lhsVal) ? (std::get<int>(lhsVal) == 0) : (std::get<float>(lhsVal) == 0.0f);
            if (lhsIsZero) return 0;
            auto rhsVal = evaluateCompileTimeConstant(binaryExpr->getRhs().get());
            if (std::holds_alternative<std::monostate>(rhsVal)) return std::monostate{};
            bool rhsIsZero = std::holds_alternative<int>(rhsVal) ? (std::get<int>(rhsVal) == 0) : (std::get<float>(rhsVal) == 0.0f);
            return (lhsIsZero || rhsIsZero) ? 0 : 1;
        }
        
        if (binaryExpr->getBinaryOpType() == AST::BinaryOpType::OR) {
            auto lhsVal = evaluateCompileTimeConstant(binaryExpr->getLhs().get());
            if (std::holds_alternative<std::monostate>(lhsVal)) return std::monostate{};
            bool lhsIsZero = std::holds_alternative<int>(lhsVal) ? (std::get<int>(lhsVal) == 0) : (std::get<float>(lhsVal) == 0.0f);
            if (!lhsIsZero) return 1;
            auto rhsVal = evaluateCompileTimeConstant(binaryExpr->getRhs().get());
            if (std::holds_alternative<std::monostate>(rhsVal)) return std::monostate{};
            bool rhsIsZero = std::holds_alternative<int>(rhsVal) ? (std::get<int>(rhsVal) == 0) : (std::get<float>(rhsVal) == 0.0f);
            return (lhsIsZero && rhsIsZero) ? 0 : 1;
        }
        
        auto lhsVal = evaluateCompileTimeConstant(binaryExpr->getLhs().get());
        auto rhsVal = evaluateCompileTimeConstant(binaryExpr->getRhs().get());
        
        if (std::holds_alternative<std::monostate>(lhsVal) || std::holds_alternative<std::monostate>(rhsVal)) {
            return std::monostate{};
        }
        
        // 判断结果类型：如果有 float 则结果为 float
        bool isFloatResult = std::holds_alternative<float>(lhsVal) || std::holds_alternative<float>(rhsVal);
        
        switch (binaryExpr->getBinaryOpType()) {
            case AST::BinaryOpType::ADD:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs + rhs;
                } else {
                    return std::get<int>(lhsVal) + std::get<int>(rhsVal);
                }
            case AST::BinaryOpType::SUB:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs - rhs;
                } else {
                    return std::get<int>(lhsVal) - std::get<int>(rhsVal);
                }
            case AST::BinaryOpType::MUL:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs * rhs;
                } else {
                    return std::get<int>(lhsVal) * std::get<int>(rhsVal);
                }
            case AST::BinaryOpType::DIV:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    if (rhs == 0.0f) {
                        return std::monostate{};
                    }
                    return lhs / rhs;
                } else {
                    if (std::get<int>(rhsVal) == 0) {
                        return std::monostate{};
                    }
                    return std::get<int>(lhsVal) / std::get<int>(rhsVal);
                }
            case AST::BinaryOpType::MOD:
                if (isFloatResult) return std::monostate{};
                if (std::get<int>(rhsVal) == 0) {
                    return std::monostate{};
                }
                return std::get<int>(lhsVal) % std::get<int>(rhsVal);
            case AST::BinaryOpType::LT:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs < rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) < std::get<int>(rhsVal) ? 1 : 0;
                }
            case AST::BinaryOpType::GT:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs > rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) > std::get<int>(rhsVal) ? 1 : 0;
                }
            case AST::BinaryOpType::LE:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs <= rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) <= std::get<int>(rhsVal) ? 1 : 0;
                }
            case AST::BinaryOpType::GE:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs >= rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) >= std::get<int>(rhsVal) ? 1 : 0;
                }
            case AST::BinaryOpType::EQ:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs == rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) == std::get<int>(rhsVal) ? 1 : 0;
                }
            case AST::BinaryOpType::NE:
                if (isFloatResult) {
                    float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                    float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                    return lhs != rhs ? 1 : 0;
                } else {
                    return std::get<int>(lhsVal) != std::get<int>(rhsVal) ? 1 : 0;
                }
            default:
                return std::monostate{};
        }
    }
    
    return std::monostate{};
}
/*******************************************************************************************/
bool SemaCheckVisitor::isCompileTimeConstant(AST::ExprAST* expr) {
    auto constVal =  evaluateCompileTimeConstant(expr);
    if(std::holds_alternative<std::monostate>(constVal))
        return false;
    return true;
}
/*******************************************************************************************/
bool SemaCheckVisitor::checkInitValListConst(AST::InitValAST* initVal) {
    if(!initVal->isList()) { // 单个初值
        if(!isCompileTimeConstant(initVal->getExp().get())) {
            error(initVal->getLoc(), "initial value must be a compile-time constant");
            return false;
        }

        return true;
    }

    if(initVal->isEmptyList()) // 空的列表
        return true;

    // 非空列表
    for(const auto& val : initVal->getInner()) {
        bool flag = checkInitValListConst(val.get());
        if(!flag) {
            return false;
        }
    }
    return true;

}
/*******************************************************************************************/
bool SemaCheckVisitor::checkInitValListType(AST::InitValAST* initVal, AST::BType targetType) {
    if(!initVal->isList()) { // 单个初值
        if(targetType == AST::BType::INT && !initVal->getExp()->getEType().isInt()) {
            error(initVal->getLoc(), "initial value must be of type " + typeToString(targetType));
            return false;
        }
        return true;
    }

    if(initVal->isEmptyList()) // 空的列表
        return true;

    // 非空列表
    for(const auto& val : initVal->getInner()) {
        bool flag = checkInitValListType(val.get(), targetType);
        if(!flag) {
            return false;
        }
    }
    return true;
}
/*******************************************************************************************/
bool SemaCheckVisitor::checkList(const std::vector<int>& dimens, AST::InitValAST* initVal)
{
    if(!initVal->isList()) { // 单个初值
        error(initVal->getLoc(), "checkInitValList can only process the initial value  of a list");
        return false;
    }

    int num = 0;
    return checkListrec(dimens, initVal, num, 0);
}
/*******************************************************************************************/
bool SemaCheckVisitor::checkListrec(const std::vector<int>& dimens, AST::InitValAST* initVal, int& num, int depth)
{
    int originalNum = num; // 记录进入当前列表前的已填充数组元素数，用于计算需要填充0的元素数

    if(depth >= dimens.size()) // 列表的深度大于等于数组的维度，说明列表的维度与数组的维度不匹配
        return false;

    int totalElements = 1; // 数组的总元素数
    for(int dim : dimens) {
        totalElements *= dim;
    }

    if(num % dimens[dimens.size() - 1] != 0) { // 进入一个列表，判断已有元素的个数是否是最后一维的整数的倍数
        error(initVal->getLoc(), "array has insufficient initializers");
        return false;
    }

    int acc = dimens[dimens.size() - 1]; // 最长维度乘积，初始化为最后一维的元素数
    for(int i = dimens.size() - 1, temp = 1; i >= depth; i--) { // 从后往前遍历数组维度
        temp = temp * dimens[i];
        if(num % temp == 0)
            acc = temp;
        else
            break;
    } 

    // 遍历InitValAST中的每个元素
    for(const auto& val : initVal->getInner()) {
        // 单个初值
        if(!val->isList()) {
            num++;
            if(num > totalElements) { // 已填充的数组元素数大于数组的总元素数，说明数组初始化器与数组维度不匹配
                error(initVal->getLoc(), "the array initializer has too many values");
                return false;
            }
            continue;
        }
        
        // 列表
        bool flag = checkListrec(dimens, val.get(), num, depth + 1);
        if(!flag) {
            return false;
        }
    }

    int toBeFilled = acc - (num - originalNum); // 计算需要填充0的元素数
    if(toBeFilled < 0) { // 如果需要填充0的元素数小于0，说明该列表给了太多的元素，与其需填充的数组维度不匹配，
        error(initVal->getLoc(), "the array initializer has too many values");
        return false;
    }
    num += toBeFilled; // 增加需要填充0的元素数，作为当前列表的已填充元素数
    return true;
}
/*******************************************************************************************/
bool SemaCheckVisitor::flattenConstList(const std::vector<int>& dimens, AST::InitValAST* initVal, std::vector<float>& resultList) {
    if(!initVal->isList()) { // 单个初值
        error(initVal->getLoc(), "flattenList can only process the initial value  of a list");
        return false;
    }
    int num = 0;
    return flattenConstListrec(dimens, initVal, num, 0, resultList);
}
/*******************************************************************************************/
bool SemaCheckVisitor::flattenConstListrec(const std::vector<int>& dimens, AST::InitValAST* initVal, int& num, int depth, std::vector<float>& resultList) {
    int originalNum = num; // 记录进入当前列表前的已填充数组元素数，用于计算需要填充0的元素数

    if(depth >= dimens.size()) // 列表的深度大于等于数组的维度，说明列表的维度与数组的维度不匹配
        return false;

    int totalElements = 1; // 数组的总元素数
    for(int dim : dimens) {
        totalElements *= dim;
    }

    if(num % dimens[dimens.size() - 1] != 0) { // 进入一个列表，判断已有元素的个数是否是最后一维的整数的倍数
        error(initVal->getLoc(), "array has insufficient initializers");
        return false;
    }

    int acc = dimens[dimens.size() - 1]; // 最长维度乘积，初始化为最后一维的元素数
    for(int i = dimens.size() - 1, temp = 1; i >= depth; i--) { // 从后往前遍历数组维度
        temp = temp * dimens[i];
        if(num % temp == 0)
            acc = temp;
        else
            break;
    } 

    // 遍历InitValAST中的每个元素
    for(const auto& val : initVal->getInner()) {
        // 单个初值
        if(!val->isList()) {
            float elementVal;
            auto constValue = evaluateCompileTimeConstant(val->getExp().get());
            elementVal = std::holds_alternative<float>(constValue) ? std::get<float>(constValue) : std::get<int>(constValue);
            resultList.push_back(elementVal);
            num++;
            if(num > totalElements) { // 已填充的数组元素数大于数组的总元素数，说明数组初始化器与数组维度不匹配
                error(initVal->getLoc(), "the array initializer has too many values");
                return false;
            }
            continue;
        }
        
        // 列表
        bool flag = flattenConstListrec(dimens, val.get(), num, depth + 1, resultList);
        if(!flag) {
            return false;
        }
    }

    int toBeFilled = acc - (num - originalNum); // 计算需要填充0的元素数
    if(toBeFilled < 0) { // 如果需要填充0的元素数小于0，说明该列表给了太多的元素，与其需填充的数组维度不匹配，
        error(initVal->getLoc(), "the array initializer has too many values");
        return false;
    }
    // 填充0
    for(int i = 0; i < toBeFilled; i++) {
        resultList.push_back(0.0f);
    }
    num += toBeFilled; // 增加需要填充0的元素数，作为当前列表的已填充元素数
    return true;
}
/*******************************************************************************************/
SemaCheckVisitor::SemaCheckVisitor() {
    addBuiltinFunctions();
}
/*******************************************************************************************/
bool SemaCheckVisitor::check(const std::shared_ptr<AST::CompUnitAST>& root) {
    root->accept(*this);
    return !hasError;
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::CompUnitAST& node) {
    const auto& nodes = node.getNodes();
    for (const auto& child : nodes) {
        child->accept(*this);
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::DeclStmtAST& node) {
    const auto& varDefs = node.getVarDefs();
    for (const auto& varDef : varDefs) {
        varDef->accept(*this);
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::VarDefAST& node) { 
    const std::string& name = node.getName(); // 获取变量名
    
    if (const auto& symbol = symbolTable.lookupVarInCurrentScope(name)) { // 判断变量是否已经在当前作用域中定义过
        error(node.getLoc(), "redefinition of variable '" + name + "'");
        return;
    }

    if(node.isInited()) // 如果变量有初值
        node.getInitVal()->accept(*this);

    SymbolInfo info;
    info.kind = SymbolKind::VARIABLE;
    info.isConst = node.isConst();
    info.type = node.getType();
    info.name = name;
    info.arrayInfo.isArray = node.isArray();

    // 判断是否是全局变量（全局作用域是scopeStack中的第一个作用域）
    bool isGlobalVar = symbolTable.isGlobalScope();
    
    if(node.isConst()) { // 常量
        if(!node.isArray()) { // 常量、普通变量
            if(node.isInited()) { // 常量、普通变量、有初值
                if(node.getInitVal()->isList()) { // 使用了数组初始化器来初始化普通变量(这是错误的)
                    error(node.getLoc(), "const variable '" + name + "' must be initialized with a single value");
                } else { // 使用了单个初始数值来初始化普通变量(这是正确的)
                    // node.getInitVal()->getExp()->accept(*this); // 对赋值号右边的表达式进行语义分析
                    
                    // 判断赋值号右边的初始数值是否为编译期常量
                    if (!isCompileTimeConstant(node.getInitVal()->getExp().get())) { // 初始数值不是编译期常量（这是错误的）
                        error(node.getLoc(), "const variable '" + name + "' must be initialized with a compile-time constant");
                    } else { // 赋值号右边的初始数值为编译期常量（这是正确的）
                        AST::EType initType = node.getInitVal()->getExp()->getEType(); // 获取初始数值的类型
                        // 检查初始数值的类型和目标类型的是否一致（支持 int 和 float 之间的隐式转换）
                        if (!(initType.isNumeric() && isNumericType(info.type))) { // 初始数值的类型和目标类型不一致
                            error(node.getLoc(), "const variable '" + name + "' must be initialized with a value of type " + typeToString(info.type));
                        } else { // 初始数值的类型和目标类型的一致
                            // 存储常量的编译期常量值
                            info.constValue = evaluateCompileTimeConstant(node.getInitVal()->getExp().get());
                            // 存储常量的编译期常量初始化值
                            float constInitVal = std::holds_alternative<float>(info.constValue) ? std::get<float>(info.constValue) : std::get<int>(info.constValue);
                            node.setConstInitVal(constInitVal);
                        }
                    }
                }

            } else { // 常量、普通变量、没有初值
                error(node.getLoc(), "const variable '" + name + "' must be initialized");
            }
        } else { // 常量、数组变量
            bool indicesAllRight = true; // 标记数组定义时所有下标是否都是正确的
            // 遍历每一个数组下标
            auto& subscripts = node.getSubscripts();
            for(auto& subscript : subscripts) {
                subscript->accept(*this); // 对数组下标表达式进行语义分析
                auto expr = subscript->getExp().get();
                // 检查数组下标是否为编译期常量
                if (!isCompileTimeConstant(expr)) { // 数组下标不是编译期常量（这是错误的）
                    error(subscript->getLoc(), "array subscript must be compile-time constant");
                    indicesAllRight = false;
                    continue;
                }
                // 检查数组下标是否为整数类型
                if (!expr->getEType().isInt()) { // 数组下标不是整数类型（这是错误的）
                    error(subscript->getLoc(), "array subscript must be integer type");
                    indicesAllRight = false;
                    continue;
                }

                // 运行到这里，说明数组下标是编译期常量且是整数类型。接下来判断数组下标是否大于0
                auto constValue = evaluateCompileTimeConstant(subscript->getExp().get());
                if (std::holds_alternative<std::monostate>(constValue)) {
                    error(subscript->getLoc(), "array subscript must be compile-time constant");
                    indicesAllRight = false;
                    continue;
                } else if (std::holds_alternative<int>(constValue)) {
                    int val = std::get<int>(constValue);
                    if (val <= 0) {
                        error(subscript->getLoc(), "array subscript must be a positive integer (greater than 0), got " + std::to_string(val));
                        indicesAllRight = false;
                        continue;
                    }
                    info.arrayInfo.dimensions.push_back(val); // 将数组下标加入到符号表中
                    node.addArrayDim(val); // 将数组下标加入到VarDefAST的arrayDims中
                } else if (std::holds_alternative<float>(constValue)) {
                    error(subscript->getLoc(), "array subscript must be integer type, got float");
                    indicesAllRight = false;
                    continue;
                }
            }

            if(!indicesAllRight) {
                info.arrayInfo.isValid = false;
                symbolTable.insertVar(name, info);
                return;
            }

            // 运行到这里，说明数组下标都是正确的。接下来判断数组是否是常量数组

            if(node.isInited()) { // 常量、数组变量、有初值
                if(node.getInitVal()->isList()) { // 使用了数组初始化器来初始化数组变量(这是正确的)
                    // 检测数组初始化器中的初始数值是否为编译期常量且为数组元素的类型
                    if (checkInitValListConst(node.getInitVal().get())
                        && checkInitValListType(node.getInitVal().get(), info.type)) {
                        // checkList(info.arrayInfo.dimensions, node.getInitVal().get());
                        std::vector<float> resultList;
                        bool flattenSuccess = flattenConstList(info.arrayInfo.dimensions, node.getInitVal().get(), resultList);
                        if(flattenSuccess) {
                            node.setConstInitValList(resultList);
                        }
                    }
                } else { // 使用了单个初始数值来初始化数组变量(这是错误的)
                    error(node.getLoc(), "array variable '" + name + "' must be initialized with a list of values");
                }
            } else { // 常量、数组变量、没有初值
                error(node.getLoc(), "array variable '" + name + "' must be initialized");
            }
        }
        
    } else { // 变量
        if(!node.isArray()) { // 变量、普通变量
            if(node.isInited()) { // 变量、普通变量、有初值
                if(node.getInitVal()->isList()) { // 使用了数组初始化器来初始化普通变量(这是错误的)
                    error(node.getLoc(), "variable '" + name + "' must be initialized with a single value");
                } else { //使用了单个初始数值来初始化普通变量(这是正确的)
                    // node.getInitVal()->getExp()->accept(*this); // 对赋值号右边的表达式进行语义分析
                    
                    AST::EType initType = node.getInitVal()->getExp()->getEType(); // 获取初始数值的类型
                    // 检查初始数值的类型和目标类型的是否一致（支持 int 和 float 之间的隐式转换）
                    if (!(initType.isNumeric() && isNumericType(info.type))) { // 初始数值的类型和目标类型不一致
                        error(node.getLoc(), "variable '" + name + "' must be initialized with a value of type " + typeToString(info.type));
                    } else { // 初始数值的类型和目标类型一致
                         // 全局变量的初值表达式必须是常量表达式
                        if (isGlobalVar) {
                            if(!isCompileTimeConstant(node.getInitVal()->getExp().get())) {
                                error(node.getLoc(), "global variable '" + name + "' must be initialized with a compile-time constant");
                            } else {
                                // 计算编译期常量的值
                                auto constValue = evaluateCompileTimeConstant(node.getInitVal()->getExp().get());
                                float constVal = std::holds_alternative<float>(constValue) ? std::get<float>(constValue) : std::get<int>(constValue);
                                node.setConstInitVal(constVal);
                            }
                        }
                    }
                }
            }

        } else { // 变量、数组变量
            bool indicesAllRight = true; // 标记数组定义时所有下标是否都是正确的
            // 遍历每一个数组下标
            auto& subscripts = node.getSubscripts();
            for(auto& subscript : subscripts) {
                subscript->accept(*this); // 对数组下标表达式进行语义分析
                auto expr = subscript->getExp().get();
                // 检查数组下标是否为编译期常量
                if (!isCompileTimeConstant(expr)) { // 数组下标不是编译期常量（这是错误的）
                    error(subscript->getLoc(), "array subscript must be compile-time constant");
                    indicesAllRight = false;
                    continue;
                }
                // 检查数组下标是否为整数类型
                if (!expr->getEType().isInt()) { // 数组下标不是整数类型（这是错误的）
                    error(subscript->getLoc(), "array subscript must be integer type");
                    indicesAllRight = false;
                    continue;
                }

                // 运行到这里，说明数组下标是编译期常量且是整数类型。接下来判断数组下标是否大于0
                auto constValue = evaluateCompileTimeConstant(subscript->getExp().get());
                if (std::holds_alternative<std::monostate>(constValue)) {
                    error(subscript->getLoc(), "array subscript must be compile-time constant");
                    indicesAllRight = false;
                    continue;
                } else if (std::holds_alternative<int>(constValue)) {
                    int val = std::get<int>(constValue);
                    if (val <= 0) {
                        error(subscript->getLoc(), "array subscript must be a positive integer (greater than 0), got " + std::to_string(val));
                        indicesAllRight = false;
                        continue;
                    }
                    info.arrayInfo.dimensions.push_back(val); // 将数组下标加入到符号表中
                    node.addArrayDim(val); // 将数组下标加入到VarDefAST的arrayDims中
                } else if (std::holds_alternative<float>(constValue)) {
                    error(subscript->getLoc(), "array subscript must be integer type, got float");
                    indicesAllRight = false;
                    continue;
                }
            }

            if(!indicesAllRight) {
                info.arrayInfo.isValid = false;
                symbolTable.insertVar(name, info);
                return;
            }


            if(node.isInited()) { // 变量、数组变量、有初值
                if(node.getInitVal()->isList()) { // 使用了数组初始化器来初始化数组变量(这是正确的)
                    bool isListConst = false;
                    // 检测数组初始化器中的初始数值是否为编译期常量且为数组元素的类型
                    if (isGlobalVar) {
                        isListConst = checkInitValListConst(node.getInitVal().get());
                        if (!isListConst) {
                            error(node.getLoc(), "global array variable '" + name + "' must be initialized with compile-time constants");
                        }
                    }
                    
                    // 检测数组初始化器中的初始数值是否为数组元素的类型
                    bool isListType = checkInitValListType(node.getInitVal().get(), info.type);
                    // 全局数组变量只能使用编译期常量来初始化，且初始化器中的初始数值必须为数组元素的类型
                    if(isGlobalVar && isListConst && isListType) {
                        std::vector<float> resultList;
                        bool flattenSuccess = flattenConstList(info.arrayInfo.dimensions, node.getInitVal().get(), resultList);
                        if(flattenSuccess) {
                            node.setConstInitValList(resultList);
                        }
                    } else { // 局部数组变量只检查数组初始化器与数组定义时的维度是否匹配（结构化检查），不计算元素值（因为其元素值可能不是编译期常量），不将其扁平化
                        checkList(info.arrayInfo.dimensions, node.getInitVal().get());
                    }
                } else { // 使用了单个初始数值来初始化数组变量(这是错误的)
                    error(node.getLoc(), "array variable '" + name + "' must be initialized with a list of values");
                }
            } else { // 变量、数组变量、没有初值
                 // 数组变量没有初值，这里什么都不做
            }
        }
    }

    // 不管变量定义合法不合法，都将其加入到符号表中，而不能无视它的存在
    symbolTable.insertVar(name, info);
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::InitValAST& node) {
    if (!node.isList() && node.getExp()) {
        node.getExp()->accept(*this);
    } else if (node.isList() && !node.isEmptyList()) {
        const auto& inner = node.getInner();
        for (const auto& item : inner) {
            item->accept(*this);
        }
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::ArraySubscriptAST& node) {
    if (node.getExp()) {
        node.getExp()->accept(*this);
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::FuncDefAST& node) {
    const std::string& name = node.getName();
    
    // 直接查找函数是否已经定义
    if (const auto& symbol = symbolTable.lookupFunc(name)) {
        error(node.getLoc(), "redefinition of function '" + name + "'");
        return;
    }
    
    SymbolInfo info;
    info.kind = SymbolKind::FUNCTION;
    info.type = node.getType();
    info.name = name;
    
    currentFuncReturnType = node.getType();
    currentFuncName = name;
    
    symbolTable.enterScope();
    symbolTable.setMergeScope(true);
    
    if (!node.isEmptyParam()) {
        const auto& params = node.getParams();
        for (const auto& param : params) {
            param->accept(*this);
            
            SymbolInfo paramInfo;
            paramInfo.kind = SymbolKind::PARAMETER;
            paramInfo.type = param->getType();
            paramInfo.name = param->getName();
            
            if (param->isArray()) {
                paramInfo.arrayInfo.isArray = true;
                paramInfo.arrayInfo.dimensions.push_back(0);
                param->addArrayDim(0);
                
                if (!param->isOneDim()) {
                    const auto& subscripts = param->getSubscripts();

                    bool indicesAllRight = true; // 标记数组参数时所有下标是否都是正确的
                    for (const auto& subscript : subscripts) {
                        auto exp = subscript->getExp().get();

                        // 检查数组下标表达式是否为编译期常量
                        if (!isCompileTimeConstant(exp)) {
                            error(subscript->getLoc(), "array dimension in function parameter '" + name + "' must be a compile-time constant");
                            indicesAllRight = false;
                            continue;
                        }

                        // 检查数组下标表达式是否为整数类型
                        if(!exp->getEType().isInt()) {
                            error(subscript->getLoc(), "array dimension in function parameter '" + name + "' must be an integer type");
                            indicesAllRight = false;
                            continue;
                        }

                        // 检查数组下标表达式是否大于0
                        auto constValue = evaluateCompileTimeConstant(exp);
                        if (std::holds_alternative<std::monostate>(constValue)) {
                            error(subscript->getLoc(), "array dimension in function parameter '" + name + "' must be a compile-time constant");
                            indicesAllRight = false;
                            continue;
                        } else if (std::holds_alternative<int>(constValue)) {
                            int val = std::get<int>(constValue);
                            if (val <= 0) {
                                error(subscript->getLoc(), "array dimension in function parameter '" + name + "' must be a positive integer (greater than 0), got " + std::to_string(val));
                                indicesAllRight = false;
                                continue;
                            }
                            paramInfo.arrayInfo.dimensions.push_back(val); // 将数组下标加入到符号表中
                            param->addArrayDim(val); // 将数组下标加入到FuncFParamAST的arrayDims中
                        } else if (std::holds_alternative<float>(constValue)) {
                            error(subscript->getLoc(), "array dimension in function parameter '" + name + "' must be an integer type, got float");
                            indicesAllRight = false;
                            continue;
                        }
                    }

                    if(!indicesAllRight) {
                        paramInfo.arrayInfo.isValid = false;
                    }
                }
            }
            
            info.params.push_back(paramInfo);
            
            symbolTable.insertVar(param->getName(), paramInfo);
        }
    }

    symbolTable.insertFunc(name, info);
    
    if (node.getBody()) {
        node.getBody()->accept(*this);
    }
    
    // 这里不需要exitScope，因为BlockStmtAST中已经exitScope了

    currentFuncReturnType = AST::BType::UNDEFINED;
    currentFuncName = "";
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::FuncFParamAST& node) {
    const std::string& name = node.getName();
    
    if (const auto& symbol = symbolTable.lookupVarInCurrentScope(name)) {
        error(node.getLoc(), "duplicate parameter name '" + name + "'");
    }

    if(node.isArray()) {
        const auto& subscripts = node.getSubscripts();
        for(auto& subscript : subscripts) {
            subscript->accept(*this);
        }
    }

}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::DeclRefAST& node) {
    const std::string& name = node.getName();
    
    SymbolInfo* symbol = symbolTable.lookupVar(name);
    if (!symbol) {
        error(node.getLoc(), "undeclared variable '" + name + "'");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }
    
    if (symbol->kind == SymbolKind::FUNCTION) {
        error(node.getLoc(), "function '" + name + "' used as variable");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // 运行到这里，说明变量存在符号表中

    // 如果是int或者float类型，直接赋值给该表达式
    if (!symbol->arrayInfo.isArray && isNumericType(symbol->type)) {
        node.setEType(AST::EType(symbol->type));
        return;
    }

    // 运行到这里，说明变量是数组类型

    if(symbol->arrayInfo.isArray && !symbol->arrayInfo.isValid) {
        error(node.getLoc(), "array '" + name + "' is initialized with invalid dimensions");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // 如果是数组类型，直接赋值给该表达式
    if (symbol->arrayInfo.isArray && symbol->arrayInfo.isValid) {
        AST::EType type(symbol->type);
        type.setDims(symbol->arrayInfo.dimensions);
        node.setEType(type);
        return;
    }

    // 正常情况下不会执行到这里，但是为了确保代码的完整性，这里还是添加一个默认的赋值
    node.setEType(AST::EType(AST::BType::UNDEFINED));
    return;
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::ArrayExprAST& node) {
    const std::string& name = node.getName();
    
    SymbolInfo* symbol = symbolTable.lookupVar(name);
    if (!symbol) {
        error(node.getLoc(), "undeclared variable '" + name + "'");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    if (!symbol->arrayInfo.isArray) {
        error(node.getLoc(), "variable '" + name + "' is not an array");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }
    
    // 检查数组下标是否为整数类型
    bool indicesAllRight = true;
    const auto& indices = node.getIndices();
    for (const auto& index : indices) {
        index->accept(*this);
        AST::EType indexType = index->getExp()->getEType();
        
        if (!indexType.isInt()) {
            error(node.getLoc(), "array index must be integer type");
            indicesAllRight = false;
            break;
        }
    }

    if (!indicesAllRight) {
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // 运行到这里，说明数组下标都是整数类型

    // 检查数组定义时维度下标是否有效
    // 该数组表达式引用的数组可能是普通的局部数组变量，也可能是函数中的数组形参
    if (!symbol->arrayInfo.isValid) {
        // 下面这个例子，对于a[1]，gcc是不对它进行报错的
        // int main() {
        //      int a[b][c];
        //      a[1];
        // }    

        // error(node.getLoc(), "array '" + name + "' is initialized with invalid dimensions");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }
    
    // 运行到这里，说明数组下标都是整数类型，且数组定义时维度下标也有效

    // 如果数组表达式的维度大于数组的维度，说明数组下标超出范围
    int arrDimSize = symbol->arrayInfo.dimensions.size();
    if (indices.size() > arrDimSize) {
        error(node.getLoc(), "array index has more dimensions than the array has");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // 检查数组下标是否越界（对于编译期常量）
    for (int i = 0; i < indices.size(); ++i) {
        AST::ArraySubscriptAST* subscript = indices[i].get();
        AST::ExprAST* indexExpr = subscript->getExp().get();
       
        // 检查是否是编译期常量
        if (isCompileTimeConstant(indexExpr)) {
            auto constValue = evaluateCompileTimeConstant(indexExpr);
            if (std::holds_alternative<int>(constValue)) {
                int indexVal = std::get<int>(constValue);
                int arrayDim = symbol->arrayInfo.dimensions[i];

                // 如果是数组形参，那么第一维只要保证不为负数即可
                if(symbol->kind == SymbolKind::PARAMETER && i == 0) {
                    if(indexVal < 0)
                        error(subscript->getLoc(), "array index must be non-negative");
                    continue;
                }

    
                // 检查下标是否在有效范围内
                if (indexVal < 0 || indexVal >= arrayDim) {
                    error(node.getLoc(), "array index out of bounds: index " + std::to_string(indexVal) + " is not between 0 and " + std::to_string(arrayDim - 1));
                    // node.setEType(AST::EType(AST::BType::UNDEFINED));
                }
            }
        }
    }

    // 该数组表达式的类型
    // int a[2][3][4];
    // a[1]; // 类型为int[3][4]
    AST::EType type(symbol->type);
    int arrayExprDim = indices.size();
    int arrayDim = symbol->arrayInfo.dimensions.size();
    for (int i = arrayExprDim; i < arrayDim; ++i) {
        type.addDim(symbol->arrayInfo.dimensions[i]);
    }
    node.setEType(type);
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::CallExprAST& node) {
    const std::string& name = node.getName();
    
    // 直接查找函数是否已经定义
    SymbolInfo* symbol = symbolTable.lookupFunc(name); 
    
    if (!symbol) {
        error(node.getLoc(), "undeclared function '" + name + "'");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }
    
    if (symbol->kind != SymbolKind::FUNCTION) {
        error(node.getLoc(), "variable '" + name + "' is not a function");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // params的auto被推导为std::vector<std::shared_ptr<FuncRParamAST>>
    const auto& params = node.getParams(); // 这里的params是实参
    if (symbol->params.size() != params.size()) {
        error(node.getLoc(), "function '" + name + "' expects " + std::to_string(symbol->params.size()) + 
              " arguments, but " + std::to_string(params.size()) + " provided");
    }

    for (const auto& param : params) {
        param->accept(*this);
    }
    
    size_t paramCount = std::min(params.size(), symbol->params.size());
    for (size_t i = 0; i < paramCount; ++i) {
        // arg的auto被推导为std::shared_ptr<FuncRParamAST>
        const auto& arg = params[i]; 
        // 如果实参具有语义错误，比如使用了未定义的标识符，则arg->getEType()的BType为AST::BType::UNDEFINED
        // 在语义分析阶段，每个表达式节点的EType都被算出来了，都有值
        // 有语义错误的表达式节点的EType为AST::BType::UNDEFINED，其他表达式节点的EType为实际的类型
        AST::EType argType = arg->getEType();
        if(argType.isUndefined() || argType.isVoid()) {
            // 因为实参有语义问题，所以直接跳过，不进行类型检查
            continue;
        }
        
        SymbolInfo paramInfo = symbol->params[i]; // 形参相关信息
        AST::BType paramType = paramInfo.type; // 形参基本类型
        bool isArrayParam = paramInfo.arrayInfo.isArray; // 形参是否为数组类型
        bool isValidParam = paramInfo.arrayInfo.isValid; // 形参是否为有效数组
        if(isArrayParam && !isValidParam) { // 形参是数组类型，但是数组维度无效的
            error(node.getLoc(), "type of formal parameter " + std::to_string(i + 1) + " is incomplete");
            continue;
        }
            
        // 运行到这里，实参和形参类型都有效，开始进行类型检查

        // 检查实参类型argType是否与形参类型paramType匹配
        // 支持 int 和 float 之间的隐式转换
        
        // 如果形参类型不是数组类型而是int或者float，那么实参类型必须是int或者float，不能是数组类型，否则不能进行隐式转换
        if (!isArrayParam) {
            if(!argType.isNumeric()) {
                error(node.getLoc(), "argument type mismatch for parameter " + std::to_string(i + 1) + 
                      " of function " + name);
            }
            continue;
        }

        // 如果形参类型是数组类型，那么实参类型必须是数组类型
        if(isArrayParam) { // 形参是数组类型
            if(!argType.isArray()) { // 如果实参类型不是数组类型，则报错
                error(node.getLoc(), "argument type mismatch for parameter " + std::to_string(i + 1) + 
                      " of function " + name);
                continue;
            }

            // 运行到这里，说明实参类型是数组类型，检查数组元素类型是否匹配
            if(argType.getType() != paramType) { // 如果实参类型数组元素类型与形参类型数组元素类型不一致，则报错
                error(node.getLoc(), "argument type mismatch for parameter " + std::to_string(i + 1) + 
                      " of function " + name);
                continue;
            }
            
            // 继续检查数组维度是否匹配
            int argDimCount = argType.getDims().size();
            int paramDimCount = paramInfo.arrayInfo.dimensions.size();
            if(argDimCount != paramDimCount) { // 如果实参类型维度个数与形参类型维度个数不一致，则报错
                error(node.getLoc(), "argument type mismatch for parameter " + std::to_string(i + 1) + 
                      " of function " + name);
                continue;
            }

            // 运行到这里，说明实参类型维度个数与形参类型维度个数一致，检查每一个维度是否匹配

            // 形参和实参是一维数组，就不检查维度了
            // func(int a[]);
            // int a[2]
            // int b[3];
            // func(a); // OK
            // func(b); // OK
            if(argDimCount == 1 && paramDimCount == 1) 
                continue;

            // 形参和实参不是一维数组，检查维度是否匹配
            // int arr[2][3][4];
            // int func(int a[][3][4]);
            // func(arr); 
            // arr 的类型是 int[2][3][4]，argDimCount = 3 (作为参数传递时，会被转换为 int (*a)[3][4])
            // a的类型是 int (*)[3][4]，paramDimCount = 2
            const auto& argDims = argType.getDims();
            const auto& paramDims = paramInfo.arrayInfo.dimensions;
            
            for(int j = 1; j < argDimCount; ++j) {
                if(argDims[j] != paramDims[j]) { // 如果实参类型维度与形参类型维度不一致，则报错
                    error(node.getLoc(), "argument type mismatch for parameter " + std::to_string(i + 1) + 
                          " of function " + name);
                    // 只要检查到有一个维度下标不一致，就报错，不需要继续检查后面的维度
                    break;
                }
            }
        }
    }

    node.setEType(AST::EType(symbol->type));
    return;
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::FuncRParamAST& node) {
    if (node.getExp()) {
        node.getExp()->accept(*this);
    }
    node.setEType(AST::EType(node.getExp()->getEType()));
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::BinaryExprAST& node) {
    node.getLhs()->accept(*this);
    node.getRhs()->accept(*this);

    AST::EType lType = node.getLhs()->getEType();
    AST::EType rType = node.getRhs()->getEType();

    AST::BinaryOpType op = node.getBinaryOpType();

    if (op == AST::BinaryOpType::ASSIGN) {
        auto lhs = node.getLhs().get();
        
        if (auto declRef = llvm::dyn_cast<AST::DeclRefAST>(lhs)) {
            SymbolInfo* symbol = symbolTable.lookupVar(declRef->getName());
            if (symbol && symbol->isConst) {
                error(node.getLoc(), "cannot assign to const variable '" + declRef->getName() + "'");
            }
            // 当DeclRefAST表示数组变量名时，方括号个数必须和数组变量的维数相同（即定位到元素）
            // 假如int a[2][2]，那么a在赋值号左边就必须是 a[0][0]、a[0][1]、a[1][0]、a[1][1]
            // 不能是 a、a[0]、a[1]
            if (symbol && lType.isArray()) {
                error(node.getLoc(), "array index must be the same dimension as the array when assigning to an array");
            }
            if (symbol && lType.isNumeric() && !rType.isNumeric()) {
                error(node.getLoc(), "cannot assign to numeric type from non-numeric type");
            }
            
        } else if (auto arrayExpr = llvm::dyn_cast<AST::ArrayExprAST>(lhs)) {
            SymbolInfo* symbol = symbolTable.lookupVar(arrayExpr->getName());
            if (symbol && symbol->isConst) {
                error(node.getLoc(), "cannot assign to const array '" + arrayExpr->getName() + "'");
            }
            // 当ArrayExprAST表示数组元素访问时，方括号个数必须和数组变量的维数相同（即定位到元素）
            // if (symbol && symbol->arrayInfo.isArray) {
            //     if (symbol->arrayInfo.dims.size() != arrayExpr->getDims().size()) {
            //         error("array index must be the same dimension as the array when assigning to an array");
            //     }
            // }
            if (symbol && lType.isArray()) {
                error(node.getLoc(), "array index must be the same dimension as the array when assigning to an array");
            }

        } else {
            error(node.getLoc(), "left side of assignment must be a variable");
        }
        node.setEType(lType);
        return;
    }

    // 对于下面的运算符来说，数组类型、void、未定义类型不能参与运算
    bool isArray = lType.isArray() || rType.isArray();
    bool isVoid = lType.isVoid() || rType.isVoid();
    bool isUndefined = lType.isUndefined() || rType.isUndefined();
    if (isArray || isVoid || isUndefined) {
        error(node.getLoc(), "array, void, or undefined type expression cannot be used in binary operation");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }

    // 对于下面的运算符来说，结果为int类型
    if (op == AST::BinaryOpType::LT || op == AST::BinaryOpType::GT || 
        op == AST::BinaryOpType::LE || op == AST::BinaryOpType::GE ||
        op == AST::BinaryOpType::EQ || op == AST::BinaryOpType::NE ||
        op == AST::BinaryOpType::AND || op == AST::BinaryOpType::OR) {
            node.setEType(AST::EType(AST::BType::INT));
            return;
    }

    // 接下来考虑+、-、*、/、%运算符
    // 除开取模运算，如果有一个操作数是float类型，结果就是float类型
    // 取模运算符两边不能有float类型，否则结果为undefined类型
    if (lType.isFloat() || rType.isFloat()) {
        // 取模运算的操作数不能是float类型
        if(op == AST::BinaryOpType::MOD) {
            error(node.getLoc(), "invalid operands to binary % operator (have float type)");
            node.setEType(AST::EType(AST::BType::UNDEFINED));
            return;
        }        
        node.setEType(AST::EType(AST::BType::FLOAT));
    } else {
        node.setEType(AST::EType(AST::BType::INT));
    }

    // 检查整数溢出（对于编译期常量）
    if (op == AST::BinaryOpType::ADD || op == AST::BinaryOpType::SUB || op == AST::BinaryOpType::MUL || op == AST::BinaryOpType::DIV) {
        if (isCompileTimeConstant(node.getLhs().get()) && isCompileTimeConstant(node.getRhs().get())) {
            auto lhsVal = evaluateCompileTimeConstant(node.getLhs().get());
            auto rhsVal = evaluateCompileTimeConstant(node.getRhs().get());
            
            if (std::holds_alternative<int>(lhsVal) && std::holds_alternative<int>(rhsVal)) {
                int lhs = std::get<int>(lhsVal);
                int rhs = std::get<int>(rhsVal);
                AST::int32 result;
                
                switch (op) {
                    case AST::BinaryOpType::ADD:
                        if (__builtin_add_overflow(lhs, rhs, &result)) {
                            error(node.getLoc(), "integer overflow in addition");
                        }
                        break;
                    case AST::BinaryOpType::SUB:
                        if (__builtin_sub_overflow(lhs, rhs, &result)) {
                            error(node.getLoc(), "integer overflow in subtraction");
                        }
                        break;
                    case AST::BinaryOpType::MUL:
                        if (__builtin_mul_overflow(lhs, rhs, &result)) {
                            error(node.getLoc(), "integer overflow in multiplication");
                        }
                        break;
                    case AST::BinaryOpType::DIV:
                        if(lhs == INT_MIN && rhs == -1) {
                            error(node.getLoc(), "integer overflow in division");
                        }
                    default:
                        break;
                }
            } else if (std::holds_alternative<float>(lhsVal) && std::holds_alternative<float>(rhsVal) || 
                       std::holds_alternative<int>(lhsVal) && std::holds_alternative<float>(rhsVal) ||
                       std::holds_alternative<float>(lhsVal) && std::holds_alternative<int>(rhsVal)) {
                float lhs = std::holds_alternative<int>(lhsVal) ? static_cast<float>(std::get<int>(lhsVal)) : std::get<float>(lhsVal);
                float rhs = std::holds_alternative<int>(rhsVal) ? static_cast<float>(std::get<int>(rhsVal)) : std::get<float>(rhsVal);
                float result;
                
                switch (op) {
                    case AST::BinaryOpType::ADD:
                        result = lhs + rhs;
                        if (std::isinf(result)) {
                            error(node.getLoc(), "float overflow in addition");
                        }
                        break;
                    case AST::BinaryOpType::SUB:
                        result = lhs - rhs;
                        if (std::isinf(result)) {
                            error(node.getLoc(), "float overflow in subtraction");
                        }
                        break;
                    case AST::BinaryOpType::MUL:
                        result = lhs * rhs;
                        if (std::isinf(result)) {
                            error(node.getLoc(), "float overflow in multiplication");
                        }
                        break;
                    case AST::BinaryOpType::DIV:
                        result = lhs / rhs;
                        if(std::isinf(result)) {
                            error(node.getLoc(), "float overflow in division");
                        }
                    default:
                        break;
                }
            }
        }
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::UnaryExprAST& node) {
    node.getExp()->accept(*this);

    // 一元表达式中操作数必须int或者float类型的表达式
    AST::EType type = node.getExp()->getEType();
    if (!type.isNumeric()) {
        error(node.getLoc(), "unary operator must be applied to int or float type");
        node.setEType(AST::EType(AST::BType::UNDEFINED));
        return;
    }
    node.setEType(type);

    // 检查浮点数编译期常量的溢出
    AST::UnaryOpType op = node.getUnaryOpType();
    if(op == AST::UnaryOpType::MINUS) {
        if(isCompileTimeConstant(node.getExp().get())) {
            auto val = evaluateCompileTimeConstant(node.getExp().get());
            if(std::holds_alternative<int>(val)) {
                int value = std::get<int>(val);
                if(value == INT_MIN) {
                    error(node.getLoc(), "int overflow in unary minus");
                }
            } else if (std::holds_alternative<float>(val)) {
                float value = std::get<float>(val);
                float result = -value;
                if (std::isinf(result)) {
                    error(node.getLoc(), "float overflow in unary minus");
                }
            }
        }
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::IntLiteralAST& node) {
    node.setEType(AST::EType(AST::BType::INT));
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::FloatLiteralAST& node) {
    node.setEType(AST::EType(AST::BType::FLOAT));
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::BlockStmtAST& node) {
    symbolTable.enterScope();
    
    const auto& items = node.getItems();
    for (const auto& item : items) {
        item->accept(*this);
    }
    
    symbolTable.exitScope();
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::IfStmtAST& node) {
    if (node.getCond()) {
        node.getCond()->accept(*this);
    }
    
    if (node.getThenBody()) {
        node.getThenBody()->accept(*this);
    }
    
    if (node.hasElse() && node.getElseBody()) {
        node.getElseBody()->accept(*this);
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::WhileStmtAST& node) {
    // 其实不用加判断的，WhileStmtAST肯定有条件表达式和循环体
    // 只不过循环体可能是NullStmtAST

    if (node.getCondition()) {
        node.getCondition()->accept(*this);
    }
    
    loopDepth++;
    if (node.getBody()) {
        node.getBody()->accept(*this);
    }
    loopDepth--;
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::ReturnStmtAST& node) {
    if(node.hasReturnExpr())
        node.getReturnExpr()->accept(*this);

    if (currentFuncReturnType == AST::BType::VOID) { // void 函数

        // void func() {}
        // void func1() {
        //     return func(); // 这是可以的
        // }
        //
        // 以上例子说明，void函数可以有返回值，但是返回值的类型必须是void类型，否则会报错
        if (node.hasReturnExpr() && ! node.getReturnExpr()->getEType().isVoid()) {
            error(node.getLoc(), "void function '" + currentFuncName + "' should not return a value");
        }
    } else { // 非 void 函数
        if (!node.hasReturnExpr()) { // 非 void 函数且无返回值
            error(node.getLoc(), "non-void function '" + currentFuncName + "' must return a value");
        } else { // 非 void 函数且有返回值
            // 其实这里不用检查返回值类型是否与函数返回类型匹配
            // 函数返回值类型必定是int或者float类型，不可能是数组类型、void类型、undefined类型
            // int和float之间可以进行隐式转换
            AST::EType type = node.getReturnExpr()->getEType();
            if(type.isArray() || type.isVoid() || type.isUndefined()) {
                error(node.getLoc(), "return statement must return a value of the same type as the function");
            }
        }
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::BreakStmtAST& node) {
    if (loopDepth == 0) {
        error(node.getLoc(), "'break' statement not in loop");
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::ContinueStmtAST& node) {
    if (loopDepth == 0) {
        error(node.getLoc(), "'continue' statement not in loop");
    }
}
/*******************************************************************************************/
void SemaCheckVisitor::visit(AST::NullStmtAST& node) {
}
/*******************************************************************************************/
void SemaCheckVisitor::addBuiltinFunctions() {
    // 我有一个想法，利用json文件来存储、配置这些库函数
    // 程序运行时，读取这个json文件进行解析，就不用像我们现在在这里一个个配置这些函数
    // 而且，利用json还能增加配置库函数，不像这里，一旦要增加一个库函数，整个编译器程序都要重新编译

    // (1) int getint();
    SymbolInfo getint;
    getint.kind = SymbolKind::FUNCTION;
    getint.type = AST::BType::INT;
    getint.name = "getint";
    symbolTable.insertFunc("getint", getint);

    // (2) int getch();
    SymbolInfo getch;
    getch.kind = SymbolKind::FUNCTION;
    getch.type = AST::BType::INT;
    getch.name = "getch";
    symbolTable.insertFunc("getch", getch);

    // (3) float getfloat();
    SymbolInfo getfloat;
    getfloat.kind = SymbolKind::FUNCTION;
    getfloat.type = AST::BType::FLOAT;
    getfloat.name = "getfloat";
    symbolTable.insertFunc("getfloat", getfloat);

    // (4) int getarray(int[]);
    SymbolInfo getarray;
    getarray.kind = SymbolKind::FUNCTION;
    getarray.type = AST::BType::INT;
    getarray.name = "getarray";
    {
        SymbolInfo param;
        param.kind = SymbolKind::PARAMETER;
        param.type = AST::BType::INT;
        param.arrayInfo.isArray = true;
        param.arrayInfo.dimensions.push_back(0);
        getarray.params.push_back(param);
    }
    symbolTable.insertFunc("getarray", getarray);

    // (5) int getfarray(float[]);
    SymbolInfo getfarray;
    getfarray.kind = SymbolKind::FUNCTION;
    getfarray.type = AST::BType::INT;
    getfarray.name = "getfarray";
    {
        SymbolInfo param;
        param.kind = SymbolKind::PARAMETER;
        param.type = AST::BType::FLOAT;
        param.arrayInfo.isArray = true;
        param.arrayInfo.dimensions.push_back(0);
        getfarray.params.push_back(param);
    }
    symbolTable.insertFunc("getfarray", getfarray);

    // (6) void putint(int);
    SymbolInfo putint;
    putint.kind = SymbolKind::FUNCTION;
    putint.type = AST::BType::VOID;
    putint.name = "putint";
    {
        SymbolInfo param;
        param.kind = SymbolKind::PARAMETER;
        param.type = AST::BType::INT;
        param.arrayInfo.isArray = false;
        putint.params.push_back(param);
    }
    symbolTable.insertFunc("putint", putint);

    // (7) void putch(int);
    SymbolInfo putch;
    putch.kind = SymbolKind::FUNCTION;
    putch.type = AST::BType::VOID;
    putch.name = "putch";
    {
        SymbolInfo param;
        param.kind = SymbolKind::PARAMETER;
        param.type = AST::BType::INT;
        param.arrayInfo.isArray = false;
        putch.params.push_back(param);
    }
    symbolTable.insertFunc("putch", putch);

    // (8) void putfloat(float);
    SymbolInfo putfloat;
    putfloat.kind = SymbolKind::FUNCTION;
    putfloat.type = AST::BType::VOID;
    putfloat.name = "putfloat";
    {
        SymbolInfo param;
        param.kind = SymbolKind::PARAMETER;
        param.type = AST::BType::FLOAT;
        param.arrayInfo.isArray = false;
        putfloat.params.push_back(param);
    }
    symbolTable.insertFunc("putfloat", putfloat);

    // (9) void putarray(int, int[]);
    SymbolInfo putarray;
    putarray.kind = SymbolKind::FUNCTION;
    putarray.type = AST::BType::VOID;
    putarray.name = "putarray";
    {
        SymbolInfo param1;
        param1.kind = SymbolKind::PARAMETER;
        param1.type = AST::BType::INT;
        param1.arrayInfo.isArray = false;
        putarray.params.push_back(param1);
        
        SymbolInfo param2;
        param2.kind = SymbolKind::PARAMETER;
        param2.type = AST::BType::INT;
        param2.arrayInfo.isArray = true;
        param2.arrayInfo.dimensions.push_back(0);
        putarray.params.push_back(param2);
    }
    symbolTable.insertFunc("putarray", putarray);

    // (10) void putfarray(int, float[]);
    SymbolInfo putfarray;
    putfarray.kind = SymbolKind::FUNCTION;
    putfarray.type = AST::BType::VOID;
    putfarray.name = "putfarray";
    {
        SymbolInfo param1;
        param1.kind = SymbolKind::PARAMETER;
        param1.type = AST::BType::INT;
        param1.arrayInfo.isArray = false;
        putfarray.params.push_back(param1);
        
        SymbolInfo param2;
        param2.kind = SymbolKind::PARAMETER;
        param2.type = AST::BType::FLOAT;
        param2.arrayInfo.isArray = true;
        param2.arrayInfo.dimensions.push_back(0);
        putfarray.params.push_back(param2);
    }
    symbolTable.insertFunc("putfarray", putfarray);

    // (11) void putf(<格式串>, int, ...);
    // 这个函数我还没处理，先不实现（未完成）

    // (12) void starttime();
    SymbolInfo starttime;
    starttime.kind = SymbolKind::FUNCTION;
    starttime.type = AST::BType::VOID;
    starttime.name = "starttime";
    symbolTable.insertFunc("starttime", starttime);

    // (13) void stoptime();
    SymbolInfo stoptime;
    stoptime.kind = SymbolKind::FUNCTION;
    stoptime.type = AST::BType::VOID;
    stoptime.name = "stoptime";
    symbolTable.insertFunc("stoptime", stoptime);
}
/***************************************************************************/
} // namespace Sema
