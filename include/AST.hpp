#ifndef SYSY2022_AST_HPP
#define SYSY2022_AST_HPP

#include <memory>
#include <vector>
#include <string>
#include <cstdint> // just for int32_t
#include <stdexcept> // just for std::invalid_argument
#include <optional> // just for std::optional

#include "Types.hpp"
#include "location.hpp"

namespace AST {
// 前向声明
class ASTNode; // 抽象语法树节点基类
class ASTVisitor; // 访问者基类

class CompUnitAST; // 编译单元节点
class VarDefAST; // 变量定义节点
class DeclStmtAST; // 声明语句节点
class InitValAST; // 初始化值节点
class ArraySubscriptAST; // 数组下标节点

class FuncDefAST; // 函数定义节点
class FuncFParamAST; // 函数参数节点

class ExprAST; // 表达式节点基类
class DeclRefAST; // 变量名引用节点
class ArrayExprAST; // 数组元素访问节点
class CallExprAST; // 函数调用表达式节点
class FuncRParamAST; // 函数实参节点
class BinaryExprAST; // 二元表达式节点
class UnaryExprAST; // 一元表达式节点
class IntLiteralAST; // 整型字面量节点
class FloatLiteralAST; // 浮点型字面量节点

using StmtAST = ASTNode; // 语句节点基类
class BlockStmtAST; // 语句块节点
class IfStmtAST; // 条件语句节点
class WhileStmtAST; // 循环语句节点
class NullStmtAST; // 空语句节点
class BreakStmtAST; // Break语句节点
class ContinueStmtAST; // Continue语句节点
class ReturnStmtAST; // Return语句节点
//-------------------------------------------------------------------------------------------------
// 抽象语法树基类
class ASTNode {
public:
    /// AST 节点类型枚举，用于 LLVM RTTI 类型识别
    enum class NodeKind {
        NK_CompUnit,       // 编译单元节点，表示整个源文件
        NK_VarDef,         // 变量定义节点，包括常量和变量定义
        NK_DeclStmt,       // 声明语句节点，包含多个变量定义
        NK_InitVal,        // 初始化值节点，可能是表达式或初始化列表
        NK_ArraySubscript, // 数组下标节点，表示数组维度的表达式
        NK_FuncDef,        // 函数定义节点，包含函数签名和函数体
        NK_FuncFParam,     // 函数形参节点，表示函数参数声明
        NK_DeclRef,        // 变量引用节点，表示对变量的引用
        NK_ArrayExpr,      // 数组访问表达式节点，表示数组元素访问
        NK_CallExpr,       // 函数调用表达式节点
        NK_FuncRParam,     // 函数实参节点，表示函数调用的参数
        NK_BinaryExpr,     // 二元表达式节点，如加减乘除等运算
        NK_UnaryExpr,      // 一元表达式节点，如正负号、逻辑非等
        NK_IntLiteral,     // 整型字面量节点
        NK_FloatLiteral,   // 浮点型字面量节点
        NK_BlockStmt,      // 语句块节点，包含多个语句
        NK_IfStmt,         // 条件语句节点
        NK_WhileStmt,      // 循环语句节点
        NK_NullStmt,       // 空语句节点，表示单独的分号
        NK_BreakStmt,      // Break 语句节点
        NK_ContinueStmt,   // Continue 语句节点
        NK_ReturnStmt      // Return 语句节点
    };

private:
    NodeKind kind_;
    yy::location loc_;

protected:
    ASTNode(NodeKind kind) : kind_(kind) {}

public:
    yy::location getLoc() const { return loc_; }
    void setLoc(yy::location loc) { loc_ = loc; }
    NodeKind getKind() const { return kind_; }
    virtual void accept(class ASTVisitor &visitor) = 0;
    virtual ~ASTNode() = default;
};
//-------------------------------------------------------------------------------------------------
// 访问者基类
class ASTVisitor {
public:
    virtual void visit(CompUnitAST &node) = 0;
    virtual void visit(VarDefAST &node) = 0;
    virtual void visit(DeclStmtAST &node) = 0;
    virtual void visit(InitValAST &node) = 0;
    virtual void visit(ArraySubscriptAST &node) = 0;
    virtual void visit(FuncDefAST &node) = 0;
    virtual void visit(FuncFParamAST &node) = 0;
    virtual void visit(DeclRefAST &node) = 0;
    virtual void visit(ArrayExprAST &node) = 0;
    virtual void visit(CallExprAST &node) = 0;
    virtual void visit(FuncRParamAST &node) = 0;
    virtual void visit(BinaryExprAST &node) = 0;
    virtual void visit(UnaryExprAST &node) = 0;
    virtual void visit(IntLiteralAST &node) = 0;
    virtual void visit(FloatLiteralAST &node) = 0;
    virtual void visit(BlockStmtAST &node) = 0;
    virtual void visit(IfStmtAST &node) = 0;
    virtual void visit(WhileStmtAST &node) = 0;
    virtual void visit(NullStmtAST &node) = 0;
    virtual void visit(BreakStmtAST &node) = 0;
    virtual void visit(ContinueStmtAST &node) = 0;
    virtual void visit(ReturnStmtAST &node) = 0;

    virtual ~ASTVisitor() = default;
};
//-------------------------------------------------------------------------------------------------
// 此模板将 “用next维护的节点链表” 转化为vector
// 例如：DeclStmt中的VarDef。因此，这种链表关系被两个对象维护：上一个节点和上级节点。
template <typename T>
void addNodesToVector(const std::shared_ptr<T> &head, std::vector<std::shared_ptr<T>> &outVector) {
    std::shared_ptr<T> current = head;
    while (current != nullptr) {
        outVector.push_back(current);
        current = current->next;
    }
}
//-------------------------------------------------------------------------------------------------
// 编译单元节点
class CompUnitAST: public ASTNode {
private:
    std::vector<std::shared_ptr<ASTNode>> nodes; // 存储Decl和FuncDef节点
public:
    CompUnitAST(const std::shared_ptr<ASTNode>& node) : ASTNode(NodeKind::NK_CompUnit) { nodes.push_back(node); }
    void addNode(const std::shared_ptr<ASTNode>& node) { nodes.push_back(node); }
    const std::vector<std::shared_ptr<ASTNode>>& getNodes() const { return nodes; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_CompUnit;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 变量定义节点 --- 变量定义可能是常量定义，也可能是变量定义；可能是数组定义，也可能是非数组定义；可能有初始化值，也可能没有初始化值
class VarDefAST: public ASTNode {
private:
    bool isConst_ = false; // 是否为常量定义
    BType type = BType::UNDEFINED; // 变量类型
    std::string varName; // 变量名
    bool isArray_ = false; // 是否为数组定义
    bool isInited_ = false; // 是否有初始化值，即 initvals 是否为空

    std::vector<std::shared_ptr<ArraySubscriptAST>> subscripts; // 数组定义的维度信息，非数组定义时为空
    std::shared_ptr<InitValAST> initVal;

    std::vector<int> arrayDims; // 编译期计算出的数组定义的维度信息，非数组定义时为空
    
    // 编译期常量初始化值，用于初始化非数组常量定义
    // 不管变量类型是int还是float，都用float存储
    float constInitVal = 0.0f;
    // 编译期常量数组的初始化值列表，用于初始化数组常量定义
    // 不管数组元素类型是int还是float，都用float存储
    std::vector<float> constInitValList; 

public:
    std::shared_ptr<VarDefAST> next = nullptr; // 右递归链表的下一个节点

    VarDefAST(const std::string& varName): ASTNode(NodeKind::NK_VarDef), varName(varName) {}
    VarDefAST(const std::string& varName, const std::shared_ptr<ArraySubscriptAST>& ss) : ASTNode(NodeKind::NK_VarDef), varName(varName), isArray_(true) {
        addNodesToVector(ss, subscripts);
    }
    VarDefAST(const std::string& varName, const std::shared_ptr<InitValAST>& initVal) : ASTNode(NodeKind::NK_VarDef), varName(varName), isInited_(true), initVal(initVal) {}
    VarDefAST(const std::string& varName, const std::shared_ptr<ArraySubscriptAST>& ss, const std::shared_ptr<InitValAST>& initVal)
        : ASTNode(NodeKind::NK_VarDef), varName(varName), isArray_(true), isInited_(true), initVal(initVal) {
        addNodesToVector(ss, subscripts);
    }
    void setConst() { isConst_ = true; }
    void setType(BType t) { type = t; }
    bool isConst() const { return isConst_; }
    BType getType() const { return type; }
    const std::string& getName() const { return varName; }
    bool isArray() const { return isArray_; }
    bool isInited() const { return isInited_; }

    const std::vector<std::shared_ptr<ArraySubscriptAST>>& getSubscripts() const { return subscripts; }
    const std::shared_ptr<InitValAST>& getInitVal() const { return initVal; }
    const std::vector<int>& getArrayDims() const { return arrayDims; }
    void addArrayDim(int dim) { arrayDims.push_back(dim); } // 往arrayDims中添加一个维度信息

    // 获取编译期常量初始化值
    float getConstInitVal() const { return constInitVal; }
    // 获取编译期常量数组初始化器
    const std::vector<float>& getConstInitValList() const { return constInitValList; }

    // 设置编译期常量初始化值
    void setConstInitVal(float val) { constInitVal = val; }
    // 设置编译期常量数组初始化器（已经过扁平化和填充）
    void setConstInitValList(const std::vector<float>& list) { constInitValList = list; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_VarDef;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 声明节点基类
class DeclStmtAST: public ASTNode {
private:
    bool isConst_ = false; // 是否为常量声明
    BType type = BType::UNDEFINED; // 声明的类型
    std::vector<std::shared_ptr<VarDefAST>> varDefs; // 声明的具体内容，可能是ConstDef，也可能是VarDef
public:
    DeclStmtAST(bool isConst, BType t, const std::shared_ptr<VarDefAST>& varDef) : ASTNode(NodeKind::NK_DeclStmt), isConst_(isConst), type(t) {
        addNodesToVector(varDef, varDefs);
        setAllType(t);
        if(isConst) {
            setAllConst();
        }
    }

    void setAllType(BType t) {
        for (auto& varDef : varDefs) {
            varDef->setType(t);
        }
    }

    void setAllConst() {
        for (auto& varDef : varDefs) {
            varDef->setConst();
        }
    }

    bool isConst() const { return isConst_; }
    BType getType() const { return type; }
    const std::vector<std::shared_ptr<VarDefAST>>& getVarDefs() const { return varDefs; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_DeclStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 初始化值节点 --- 初始化值可能是一个表达式，也可能是一个列表（即花括号括起来的一系列初始化值），列表中的每个元素又可能是一个表达式，也可能是另一个列表（即嵌套列表）
class InitValAST: public ASTNode {
private:
    bool isList_ = false; // 是否为列表初始化
    bool isEmptyList_ = false; // 是否为一个空的列表（仅当isList=true时有效）
    std::shared_ptr<ExprAST> exp = nullptr; // 表达式初始化的值，非表达式初始化时为nullptr
    std::vector<std::shared_ptr<InitValAST>> inner; // 列表初始化的内部元素，每个元素又是一个InitValAST节点，可能是一个表达式，也可能是另一个列表（即嵌套列表）
public:
    std::shared_ptr<InitValAST> next = nullptr; // 它的上级节点为VarDef，或者InitVal

    InitValAST(const std::shared_ptr<ExprAST>& exp) : ASTNode(NodeKind::NK_InitVal), exp(exp) {}
    InitValAST() : ASTNode(NodeKind::NK_InitVal), isList_(true), isEmptyList_(true) {} // 空列表
    InitValAST(const std::shared_ptr<InitValAST>& iv) : ASTNode(NodeKind::NK_InitVal), isList_(true)
    {
        addNodesToVector(iv, inner);
         // 合法性校验
        // 根据语言定义，InitVal的合法配置有两种情况：
        bool case1 = !isList_ && exp != nullptr; // 表达式初始化
        bool case2 = isList_ && exp == nullptr; // 列表初始化（包括空列表）
        if (!(case1 || case2)) {
            throw std::invalid_argument("Invalid InitVal configuration: isList=" + std::to_string(isList_) + ", exp=" + (exp ? "non-null" : "null"));
        }
    }

    bool isList() const { return isList_; }
    bool isEmptyList() const { return isEmptyList_; }
    const std::shared_ptr<ExprAST>& getExp() const { return exp; }
    const std::vector<std::shared_ptr<InitValAST>>& getInner() const { return inner; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_InitVal;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 数组下标节点 --- 数组下标可能是一个表达式，也可能是另一个数组下标（即多维数组的情况）
class ArraySubscriptAST: public ASTNode {
private:
    std::shared_ptr<ExprAST> exp = nullptr;

public:
    std::shared_ptr<ArraySubscriptAST> next = nullptr; // 右递归链表的下一个节点

    explicit ArraySubscriptAST(const std::shared_ptr<ExprAST>& exp) : ASTNode(NodeKind::NK_ArraySubscript), exp(exp) {}
    const std::shared_ptr<ExprAST>& getExp() const { return exp; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_ArraySubscript;
    }
    
    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 函数定义节点 --- 函数定义包含函数的返回类型、函数名、参数列表和函数体等信息
class FuncDefAST: public ASTNode {
private:
    BType type = BType::UNDEFINED; // 函数返回类型
    std::string funcName; // 函数名
    std::vector<std::shared_ptr<FuncFParamAST>> params; // 参数列表，每个参数由类型和名称组成
    std::shared_ptr<BlockStmtAST> body = nullptr; // 函数体，即一个语句块

public:
    FuncDefAST(BType t, const std::string& funcName, const std::shared_ptr<BlockStmtAST>& body) 
        : ASTNode(NodeKind::NK_FuncDef), type(t), funcName(funcName), body(body) {}
    FuncDefAST(BType t, const std::string& funcName, const std::shared_ptr<FuncFParamAST>& param, const std::shared_ptr<BlockStmtAST>& body) 
        : ASTNode(NodeKind::NK_FuncDef), type(t), funcName(funcName), body(body) {
        addNodesToVector(param, params);
    }

    BType getType() const { return type; }
    const std::string& getName() const { return funcName; }
    bool isEmptyParam() const { return params.empty(); }
    const std::vector<std::shared_ptr<FuncFParamAST>>& getParams() const { return params; }
    const std::shared_ptr<BlockStmtAST>& getBody() const { return body; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_FuncDef;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//--------------------------------------------------------------------------------------------------
// FuncFParam: int a, int b[], int c[][2]（根据语言定义，第一维一定为空）
// int a: _array=f
// int b[]: _array=t, _one_dim=t, subscripts.size()=0
// int c[][2]: _array=t, _one_dim=f, subscripts.size()=1
//
// 处理时需注意第一维的问题!!!
//
class FuncFParamAST: public ASTNode {
private:
    BType type = BType::UNDEFINED; // 参数类型
    std::string paramName; // 参数名
    bool isArray_ = false; // 是否为数组参数
    bool isOneDim_ = false; // 是否为一维数组参数（仅当isArray=true时有效）
    std::vector<std::shared_ptr<ArraySubscriptAST>> subscripts; // 数组参数的维度信息，第一维一定为空（根据语言定义）

    // 编译期计算出的数组参数的维度信息，非数组参数时为空
    // int a[]: arrayDims={0}
    // int a[][2]: arrayDims={0,2}
    // int a[][2][3]: arrayDims={0,2,3}
    std::vector<int> arrayDims;
    

public:
    std::shared_ptr<FuncFParamAST> next = nullptr; // 右递归链表的下一个节点

    /********************************************************************************************************************/
    // 1. 非数组参数
    FuncFParamAST(BType t, const std::string& paramName) 
        : ASTNode(NodeKind::NK_FuncFParam), type(t), paramName(paramName) {}
    
    // 2. 一维数组参数
    // isOneDim为true，subscripts为空
    // 在函数参数列表中：
    // int a[] 会被隐式、强制、无条件转为 int* a，此时a的类型为int*
    // int a[10] 也会被转为 int* a，写多少数字都没用，编译器直接忽略！
    //
    // 为什么要这样设计？（历史+底层原因）：
    // （1）C 语言函数不能直接传递数组
    // （2）数组太大，拷贝成本高
    // （3）只传递数组首地址（指针）效率最高
    // （4）所以 C 标准规定：数组类型自动退化为指针
    //
    // 总结：
    // （1）函数参数 int a[] → 自动变成 int*
    // （2）数组大小被忽略，无法在函数内获取
    // （3）多维数组：只有第一维变指针，后面必须写常量
    // 数组参数不是数组，是指针！第一维退化，后面维度不能丢！！！
    FuncFParamAST(BType t, const std::string& paramName, bool isOneDim)
        : ASTNode(NodeKind::NK_FuncFParam), type(t), paramName(paramName), isArray_(true), isOneDim_(true) {}

    // 3. 二维及以上数组参数，第二维及其以上维度的数组下标在subscripts中
    FuncFParamAST(BType t, const std::string& paramName, const std::shared_ptr<ArraySubscriptAST> &subscript) 
        : ASTNode(NodeKind::NK_FuncFParam), type(t), paramName(paramName), isArray_(true), isOneDim_(false) {
        addNodesToVector(subscript, subscripts);
        // 合法性校验
        // 根据语言定义，FuncFParam的合法配置有三种情况：
        bool case1 = !isArray_ && isOneDim_ && subscripts.empty(); // 非数组参数
        bool case2 = isArray_ && isOneDim_ && subscripts.empty(); // 一维
        bool case3 = isArray_ && !isOneDim_ && subscripts.size() >= 1; // 二维及以上
        if (!(case1 || case2 || case3)) {
            throw std::invalid_argument("Invalid FuncFParamAST configuration: isArray=" + std::to_string(isArray_) + ", isOneDim=" + std::to_string(isOneDim_) + ", subscripts.size()=" + std::to_string(subscripts.size()));
        }
    }
    /********************************************************************************************************************/

    BType getType() const { return type; }
    const std::string& getName() const { return paramName; }
    bool isArray() const { return isArray_; }
    bool isOneDim() const { return isOneDim_; }
    const std::vector<std::shared_ptr<ArraySubscriptAST>>& getSubscripts() const { return subscripts; }
    const std::vector<int>& getArrayDims() const { return arrayDims; }
    void addArrayDim(int dim) { arrayDims.push_back(dim); } // 往arrayDims中添加一个维度信息

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_FuncFParam;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 表达式节点基类
class ExprAST: public ASTNode {
protected:
    // 扩展类型，用于语义分析阶段，缓存类型结果，方便getExprType的计算
    // 在语义分析阶段，每个表达式节点的EType都被算出来了，都有值
    // 有语义错误的表达式节点的EType为BType::UNDEFINED，其他表达式节点的EType为实际的类型
    std::optional<EType> eType; 
    ExprAST(NodeKind kind) : ASTNode(kind) {}
public:
    // 判断eType是否有值
    bool hasEType() const { return eType.has_value(); }
    // 获取eType
    EType getEType() const { return eType.value(); }
    // 设置eType
    void setEType(const EType& eType) { this->eType = eType; }

    virtual void accept(ASTVisitor &visitor) override = 0;
    virtual ~ExprAST() = default;
};
//-------------------------------------------------------------------------------------------------
// 变量名引用节点
class DeclRefAST: public ExprAST {
private:
    std::string name;
public:
    explicit DeclRefAST(const std::string& name) : ExprAST(NodeKind::NK_DeclRef), name(name) {}
    const std::string& getName() const { return name; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_DeclRef;
    }
    
    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 数组元素访问节点 --- 数组元素访问表达式由一个变量名引用和一系列数组下标组成，表示对一个数组元素的访问
class ArrayExprAST: public ExprAST {
private:
    std::shared_ptr<DeclRefAST> ref = nullptr;
    std::vector<std::shared_ptr<ArraySubscriptAST>> indices; // index
public:
    ArrayExprAST(const std::shared_ptr<DeclRefAST>& ref, const std::shared_ptr<ArraySubscriptAST>& index) : ExprAST(NodeKind::NK_ArrayExpr), ref(ref) {
        addNodesToVector(index, indices);
    }
    const std::shared_ptr<DeclRefAST>& getRef() const { return ref; }
    const std::string& getName() const { return ref->getName(); }
    const std::vector<std::shared_ptr<ArraySubscriptAST>>& getIndices() const { return indices; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_ArrayExpr;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 函数调用表达式节点
class CallExprAST: public ExprAST {
private:
    std::shared_ptr<DeclRefAST> ref = nullptr;
    std::vector<std::shared_ptr<FuncRParamAST>> params;
public:
    CallExprAST(const std::shared_ptr<DeclRefAST>& ref): ExprAST(NodeKind::NK_CallExpr), ref(ref) {}
    CallExprAST(const std::shared_ptr<DeclRefAST>& ref, const std::shared_ptr<FuncRParamAST>& para): ExprAST(NodeKind::NK_CallExpr), ref(ref) {
        addNodesToVector(para, params);
    }

    bool isEmptyParam() const { return params.empty(); }
    std::string getName() const { return ref->getName(); }
    const std::shared_ptr<DeclRefAST>& getRef() const { return ref; }
    const std::vector<std::shared_ptr<FuncRParamAST>>& getParams() const { return params; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_CallExpr;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 函数实参节点 --- 由于函数实参列表是右递归的，所以FuncRParamAST节点通过next指针维护一个链表关系
// 链表的每个节点包含一个表达式（即一个实参）和一个指向下一个节点的指针（即下一个实参）
class FuncRParamAST: public ExprAST {
private:
    std::shared_ptr<ExprAST> exp = nullptr;
public:
    std::shared_ptr<FuncRParamAST> next = nullptr; // 右递归链表的下一个节点

    FuncRParamAST(const std::shared_ptr<ExprAST>& exp) : ExprAST(NodeKind::NK_FuncRParam), exp(exp) {}
    const std::shared_ptr<ExprAST>& getExp() const { return exp; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_FuncRParam;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};


//-------------------------------------------------------------------------------------------------
// 二元表达式节点
class BinaryExprAST: public ExprAST {
private:
    BinaryOpType op;
    std::shared_ptr<ExprAST> lhs = nullptr;
    std::shared_ptr<ExprAST> rhs = nullptr;

public:
    BinaryExprAST(BinaryOpType op, std::shared_ptr<ExprAST>& lhs, std::shared_ptr<ExprAST>& rhs) : ExprAST(NodeKind::NK_BinaryExpr), op(op), lhs(lhs), rhs(rhs) {}
    
    BinaryOpType getBinaryOpType() const { return op; }
    const std::shared_ptr<ExprAST>& getLhs() const { return lhs; }
    const std::shared_ptr<ExprAST>& getRhs() const { return rhs; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_BinaryExpr;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 一元表达式节点
class UnaryExprAST: public ExprAST {
private:
    UnaryOpType op;
    std::shared_ptr<ExprAST> exp;

public:
    UnaryExprAST(UnaryOpType op, std::shared_ptr<ExprAST> exp) : ExprAST(NodeKind::NK_UnaryExpr), op(op), exp(exp) {}
    UnaryOpType getUnaryOpType() const { return op; }
    const std::shared_ptr<ExprAST>& getExp() const { return exp; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_UnaryExpr;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 数值字面量节点基类
class NumberAST: public ExprAST {
protected:
    NumberAST(NodeKind kind) : ExprAST(kind) {}
public:
    virtual void accept(ASTVisitor &visitor) override = 0;
};
//-------------------------------------------------------------------------------------------------
// 整型字面量节点
class IntLiteralAST: public NumberAST {
private:
    int32 value;
public:
    explicit IntLiteralAST(int32 value) : NumberAST(NodeKind::NK_IntLiteral), value(value) {}
    int32 getValue() const { return value; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_IntLiteral;
    }
    
    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 浮点型字面量节点
class FloatLiteralAST: public NumberAST {
private:
    float32 value;
public:
    explicit FloatLiteralAST(float32 value) : NumberAST(NodeKind::NK_FloatLiteral), value(value) {}
    float32 getValue() const { return value; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_FloatLiteral;
    }
    
    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 语句块节点（对应优秀开源作品的CompStmt）
class BlockStmtAST: public StmtAST {
private:
    std::vector<std::shared_ptr<StmtAST>> blockItems; // Decl，Stmt
public:
    BlockStmtAST() : ASTNode(NodeKind::NK_BlockStmt), blockItems() {}
    BlockStmtAST(const std::shared_ptr<StmtAST>& blockItem) : ASTNode(NodeKind::NK_BlockStmt) {
        blockItems.push_back(blockItem);
    }

    void addItem(const std::shared_ptr<StmtAST>& stmt) { blockItems.push_back(stmt); }
    bool isEmpty() const { return blockItems.empty(); }
    const std::vector<std::shared_ptr<StmtAST>>& getItems() const { return blockItems; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_BlockStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};

//-------------------------------------------------------------------------------------------------
// 条件语句节点
class IfStmtAST: public StmtAST {
private:
    std::shared_ptr<ExprAST> cond = nullptr;
    std::shared_ptr<StmtAST> thenBody = nullptr;
    bool else_ = false;
    std::shared_ptr<StmtAST> elseBody = nullptr;
public:
    IfStmtAST(const std::shared_ptr<ExprAST>& condition, const std::shared_ptr<StmtAST>& body)
        : ASTNode(NodeKind::NK_IfStmt), cond(condition), thenBody(body), else_(false) {}
    IfStmtAST(const std::shared_ptr<ExprAST>& condition, const std::shared_ptr<StmtAST>& body,
              const std::shared_ptr<StmtAST>& elseBody)
        : ASTNode(NodeKind::NK_IfStmt), cond(condition), thenBody(body), elseBody(elseBody), else_(true) {}

    bool hasElse() const { return else_; }
    const std::shared_ptr<ExprAST>& getCond() const { return cond; }
    const std::shared_ptr<StmtAST>& getThenBody() const { return thenBody; }
    const std::shared_ptr<StmtAST>& getElseBody() const { return elseBody; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_IfStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 循环语句节点
class WhileStmtAST: public StmtAST {
private:
    std::shared_ptr<ExprAST> cond = nullptr;
    std::shared_ptr<StmtAST> body = nullptr;

public:
    WhileStmtAST(const std::shared_ptr<ExprAST>& condition, const std::shared_ptr<StmtAST>& body)
        : ASTNode(NodeKind::NK_WhileStmt), cond(condition), body(body) {}

    const std::shared_ptr<ExprAST>& getCondition() const { return cond; }
    const std::shared_ptr<StmtAST>& getBody() const { return body; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_WhileStmt;
    }

     void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// 空语句节点 --- 由于语言定义中存在空语句（即单独的分号），因此需要一个专门的AST节点来表示空语句
class NullStmtAST: public StmtAST {
public:
    NullStmtAST() : ASTNode(NodeKind::NK_NullStmt) {}

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_NullStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// Break语句节点和Continue语句节点 --- 这两种语句在语法分析阶段就可以构建AST节点了，不需要等到语义分析阶段了 
// --- 但是它们的语义分析和代码生成需要等到语义分析阶段了，因为它们需要知道它们所在的循环结构的相关信息（比如循环条件、循环体等） 
// --- 这两种语句的语义分析和代码生成需要依赖于它们所在的循环结构的信息 
class BreakStmtAST: public StmtAST {
public:
    BreakStmtAST() : ASTNode(NodeKind::NK_BreakStmt) {}

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_BreakStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
class ContinueStmtAST: public StmtAST {
public:
    ContinueStmtAST() : ASTNode(NodeKind::NK_ContinueStmt) {}

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_ContinueStmt;
    }

    void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
};
//-------------------------------------------------------------------------------------------------
// Return语句节点
class ReturnStmtAST: public StmtAST {
private:
    bool hasRetExpr = false;
    std::shared_ptr<ExprAST> retExpr = nullptr;
public:
    ReturnStmtAST() : ASTNode(NodeKind::NK_ReturnStmt), hasRetExpr(false) {}
    explicit ReturnStmtAST(const std::shared_ptr<ExprAST>& expr) : ASTNode(NodeKind::NK_ReturnStmt), hasRetExpr(true), retExpr(expr) {}
    
    bool hasReturnExpr() const { return hasRetExpr; }
    const std::shared_ptr<ExprAST>& getReturnExpr() const { return retExpr; }

    static bool classof(const ASTNode* node) {
        return node->getKind() == NodeKind::NK_ReturnStmt;
    }

     void accept(ASTVisitor &visitor) override {
        visitor.visit(*this);
    }
//-------------------------------------------------------------------------------------------------
};
} // namespace AST

#endif // SYSY2022_AST_HPP