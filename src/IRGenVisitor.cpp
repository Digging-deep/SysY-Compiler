#include "IRGenVisitor.hpp"
#include "llvm/Support/Casting.h"
#include <iostream>
#include <fstream>
#include <numeric>

namespace IRGen {
/*******************************************************************************************/
IRGenVisitor::IRGenVisitor(std::unique_ptr<llvm::LLVMContext>& context,
                                std::unique_ptr<llvm::Module>& module,
                                std::unique_ptr<llvm::IRBuilder<>>& builder,
                                Sema::SemaCheckVisitor& semaCheck) : 
                                context(context),
                                module(module),
                                builder(builder),
                                semaCheck(semaCheck)
{
    voidType = llvm::Type::getVoidTy(*context);
    intType = llvm::Type::getInt32Ty(*context);
    floatType = llvm::Type::getFloatTy(*context);
    ptrType = llvm::PointerType::get(*context, 0);
    zeroInt = llvm::ConstantInt::get(intType, llvm::APInt(32, 0));
    zeroFloat = llvm::ConstantFP::get(floatType, llvm::APFloat(0.0f));

    debugInfoGenerator = std::make_unique<AST::DebugInfoGenerator>(*context, *module, *builder);
    currentValue = nullptr;
    addBuiltinFunctions();
}
/*******************************************************************************************/
IRGenVisitor::~IRGenVisitor() {
    
}
/*******************************************************************************************/
void IRGenVisitor::setEnableDebugInfo(bool enable) {
    debugInfoGenerator->setEnableDebugInfo(enable);
}
/*******************************************************************************************/
llvm::Type* IRGenVisitor::getLLVMType(AST::BType type) {
    if(type != AST::BType::VOID && type != AST::BType::INT && type != AST::BType::FLOAT)
        throw std::logic_error("getLLVMType: type is not void, int, or float");

    switch (type) {
        case AST::BType::VOID:
            return voidType;
        case AST::BType::INT:
            return intType;
        case AST::BType::FLOAT:
            return floatType;
        default:
            return intType;
    }
}
/*******************************************************************************************/
llvm::Type* IRGenVisitor::getLLVMArrayType(llvm::Type* baseType, const std::vector<AST::int32>& dims) {
    if(!baseType->isIntegerTy(32) && !baseType->isFloatTy())
        throw std::logic_error("getLLVMArrayType: baseType is not int or float");

    llvm::Type* arrayType = baseType;
    
    for (auto it = dims.rbegin(); it != dims.rend(); ++it) {
        arrayType = llvm::ArrayType::get(arrayType, *it);
    }
    
    return arrayType;
}
/*******************************************************************************************/
llvm::Value* IRGenVisitor::createCast(llvm::Value* value, llvm::Type* toType) {
    if(!value || !toType)
        throw std::logic_error("createCast: value or toType is null");
    if(!toType->isIntegerTy(1) && !toType->isIntegerTy(32) && !toType->isFloatTy())
        throw std::logic_error("createCast: toType is not bool, int, or float");
        
    if (value->getType() == toType) return value;

    if(toType->isIntegerTy(1))
        return convertToBool(value);
    if(toType->isIntegerTy(32))
        return convertToInt(value);    
    if(toType->isFloatTy())
        return convertToFloat(value);

    return value;
}
/*******************************************************************************************/
llvm::Value* IRGenVisitor::convertToBool(llvm::Value* value) {
    if (value->getType()->isIntegerTy(1)) {
        return value;
    }
    if (value->getType()->isIntegerTy(32)) {
        return builder->CreateICmpNE(value, zeroInt, "tobool");
    }
    if (value->getType()->isFloatTy()) {
        return builder->CreateFCmpUNE(value, zeroFloat, "tobool");
    }

    // 如果是其他类型，则抛出异常
    throw std::logic_error("convertToBool: value type is not bool, int, or float");
}
/*******************************************************************************************/
llvm::Value* IRGenVisitor::convertToInt(llvm::Value* value) {
    if (value->getType()->isIntegerTy(32)) {
        return value;
    }
    if (value->getType()->isIntegerTy(1)) {
        return builder->CreateZExt(value, intType, "toint");
    }
    if (value->getType()->isFloatTy()) {
        return builder->CreateFPToSI(value, intType, "toint");
    }
    // 如果是其他类型，则抛出异常
    throw std::logic_error("convertToInt: value type is not bool, int, or float");
}
/*******************************************************************************************/
llvm::Value* IRGenVisitor::convertToFloat(llvm::Value* value) {
    if (value->getType()->isFloatTy()) {
        return value;
    }
    if (value->getType()->isIntegerTy(1)) {
        auto *i32Val = builder->CreateZExt(value, intType, "toInt");
        return builder->CreateSIToFP(i32Val, floatType, "tofloat");
    }
    if (value->getType()->isIntegerTy(32)) {
        return builder->CreateSIToFP(value, floatType, "tofloat");
    }
    throw std::logic_error("convertToFloat: value type is not bool, int, or float");
}
/*******************************************************************************************/
llvm::AllocaInst* IRGenVisitor::createAlloca(llvm::Type* type, llvm::Value* arraySize) {
    llvm::IRBuilder<> tmpBuilder(allocaInsertBlock, allocaInsertPoint);
    llvm::AllocaInst* alloca = tmpBuilder.CreateAlloca(type, arraySize);
    return alloca;
}
/*******************************************************************************************/
void IRGenVisitor::flattenList(llvm::Type* baseType, const std::vector<AST::int32>& dimens, AST::InitValAST* initVal, std::vector<llvm::Value*>& resultList) {
    if(!initVal->isList()) { // 单个初值
        throw std::logic_error("flattenList can only process the initial value  of a list");
    }
    AST::int32 num = 0;
    flattenListrec(baseType, dimens, initVal, num, 0, resultList);
}
/*******************************************************************************************/
void IRGenVisitor::flattenListrec(llvm::Type* baseType, const std::vector<AST::int32>& dimens, AST::InitValAST* initVal, AST::int32& num, AST::int32 depth, std::vector<llvm::Value*>& resultList) {
    AST::int32 originalNum = num; // 记录进入当前列表前的已填充数组元素数，用于计算需要填充0的元素数

    if(depth >= dimens.size()) {// 列表的深度（从0开始计数）大于等于数组的维度个数，说明列表的维度与数组的维度不匹配
        throw std::logic_error("flattenList: list dimension does not match array dimension");
    }

    AST::int32 totalElements = std::accumulate(dimens.begin(), dimens.end(), 1, std::multiplies<AST::int32>()); // 计算数组元素总数

    if(num % dimens[dimens.size() - 1] != 0) { // 进入一个列表，判断已有元素的个数是否是最后一维的整数的倍数
        throw std::logic_error("flattenList: list dimension does not match array dimension");
    }

    AST::int32 acc = dimens[dimens.size() - 1]; // 最长维度乘积，初始化为最后一维的元素数
    for(AST::int32 i = dimens.size() - 1, temp = 1; i >= depth; i--) { // 从后往前遍历数组维度
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
            val->accept(*this);
            if(!currentValue) {
                throw std::logic_error("the array initializer has a null value");
            }

            if(currentValue->getType() != baseType) {
                currentValue = createCast(currentValue, baseType);
            }
            resultList.push_back(currentValue);
            num++;
            if(num > totalElements) { // 已填充的数组元素数大于数组的总元素数，说明数组初始化器与数组维度不匹配
                throw std::logic_error("the array initializer has too many values");
            }
            continue;
        }

        // 列表
        flattenListrec(baseType, dimens, val.get(), num, depth + 1, resultList);
    }

    AST::int32 toBeFilled = acc - (num - originalNum); // 计算需要填充0的元素数
    if(toBeFilled < 0) { // 如果需要填充0的元素数小于0，说明该列表给了太多的元素，与其需填充的数组维度不匹配，
        throw std::logic_error("the array initializer has too many values");
    }
    // 填充0
    for(AST::int32 i = 0; i < toBeFilled; i++) {
        resultList.push_back(llvm::Constant::getNullValue(baseType));
    }
    num += toBeFilled; // 增加需要填充0的元素数，作为当前列表的已填充元素数
}
/*******************************************************************************************/
void IRGenVisitor::addBuiltinFunctions() {
    // (1) int getint();
    llvm::FunctionType* getintType = llvm::FunctionType::get(intType, false);
    llvm::Function* getintFunc = llvm::Function::Create(getintType, llvm::Function::ExternalLinkage, "getint", module.get());
    symbolTable.setFunction("getint", FunctionInfo(getintFunc));
    
    // (2) int getch();
    llvm::FunctionType* getchType = llvm::FunctionType::get(intType, false);
    llvm::Function* getchFunc = llvm::Function::Create(getchType, llvm::Function::ExternalLinkage, "getch", module.get());
    symbolTable.setFunction("getch", FunctionInfo(getchFunc));

    // (3) float getfloat();
    llvm::FunctionType* getfloatType = llvm::FunctionType::get(floatType, false);
    llvm::Function* getfloatFunc = llvm::Function::Create(getfloatType, llvm::Function::ExternalLinkage, "getfloat", module.get());
    symbolTable.setFunction("getfloat", FunctionInfo(getfloatFunc));

    // (4) int getarray(int[]);
    llvm::FunctionType* getarrayType = llvm::FunctionType::get(intType, {ptrType}, false);
    llvm::Function* getarrayFunc = llvm::Function::Create(getarrayType, llvm::Function::ExternalLinkage, "getarray", module.get());
    symbolTable.setFunction("getarray", FunctionInfo(getarrayFunc));
    
    // (5) int getfarray(float[]);
    llvm::FunctionType* getfarrayType = llvm::FunctionType::get(intType, {ptrType}, false);
    llvm::Function* getfarrayFunc = llvm::Function::Create(getfarrayType, llvm::Function::ExternalLinkage, "getfarray", module.get());
    symbolTable.setFunction("getfarray", FunctionInfo(getfarrayFunc));
    
    // (6) void putint(int);
    llvm::FunctionType* putintType = llvm::FunctionType::get(voidType, {intType}, false);
    llvm::Function* putintFunc = llvm::Function::Create(putintType, llvm::Function::ExternalLinkage, "putint", module.get());
    symbolTable.setFunction("putint", FunctionInfo(putintFunc));
    
    // (7) void putch(int);    
    llvm::FunctionType* putchType = llvm::FunctionType::get(voidType, {intType}, false);
    llvm::Function* putchFunc = llvm::Function::Create(putchType, llvm::Function::ExternalLinkage, "putch", module.get());
    symbolTable.setFunction("putch", FunctionInfo(putchFunc));
    
    // (8) void putfloat(float);
    llvm::FunctionType* putfloatType = llvm::FunctionType::get(voidType, {floatType}, false);
    llvm::Function* putfloatFunc = llvm::Function::Create(putfloatType, llvm::Function::ExternalLinkage, "putfloat", module.get());
    symbolTable.setFunction("putfloat", FunctionInfo(putfloatFunc));

    // (9) void putarray(int, int[]);
    std::vector<llvm::Type*> putarrayArgs = {intType, ptrType};
    llvm::FunctionType* putarrayType = llvm::FunctionType::get(voidType, putarrayArgs, false);
    llvm::Function* putarrayFunc = llvm::Function::Create(putarrayType, llvm::Function::ExternalLinkage, "putarray", module.get());
    symbolTable.setFunction("putarray", FunctionInfo(putarrayFunc));
    
    // (10) void putfarray(int, float[]);
    std::vector<llvm::Type*> putfarrayArgs = {intType, ptrType};
    llvm::FunctionType* putfarrayType = llvm::FunctionType::get(voidType, putfarrayArgs, false);
    llvm::Function* putfarrayFunc = llvm::Function::Create(putfarrayType, llvm::Function::ExternalLinkage, "putfarray", module.get());
    symbolTable.setFunction("putfarray", FunctionInfo(putfarrayFunc));
    
    // (11) void putf(<格式串>, int, ...);
    // 这个函数我还没处理，先不实现（未完成）

    // (12) void starttime();
    llvm::FunctionType* starttimeType = llvm::FunctionType::get(voidType, false);
    llvm::Function* starttimeFunc = llvm::Function::Create(starttimeType, llvm::Function::ExternalLinkage, "_sysy_starttime", module.get());
    symbolTable.setFunction("starttime", FunctionInfo(starttimeFunc));
    
    // (13) void stoptime();
    llvm::FunctionType* stoptimeType = llvm::FunctionType::get(voidType, false);
    llvm::Function* stoptimeFunc = llvm::Function::Create(stoptimeType, llvm::Function::ExternalLinkage, "_sysy_stoptime", module.get());
    symbolTable.setFunction("stoptime", FunctionInfo(stoptimeFunc));
}
/*******************************************************************************************/
bool IRGenVisitor::generate(const std::shared_ptr<AST::CompUnitAST>& root) {
    if(semaCheck.hasErrors())
        throw std::logic_error("IR can only be generated after semantic analysis.");

    // 初始化调试信息
    debugInfoGenerator->initialize();
    
    // 生成代码
    root->accept(*this);
    
    // 完成调试信息构建
    debugInfoGenerator->finalize();
    
    return true;
}
/*******************************************************************************************/
void IRGenVisitor::printIR() {
    module->print(llvm::outs(), nullptr);
}
/*******************************************************************************************/
void IRGenVisitor::printIR(std::ostream& os) {
    std::string str;
    llvm::raw_string_ostream rso(str);
    module->print(rso, nullptr);
    os << str;
}
/*******************************************************************************************/
void IRGenVisitor::saveIRToFile(const std::string& filename) {
    std::string irString;
    llvm::raw_string_ostream rso(irString);
    module->print(rso, nullptr);
    
    std::ofstream outfile(filename);
    if (outfile.is_open()) {
        outfile << irString;
        outfile.close();
        llvm::outs() << "LLVM IR saved to " << filename << "\n";
    } else {
        llvm::errs() << "Could not open file: " << filename << "\n";
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::CompUnitAST& node) {
    const auto& nodes = node.getNodes();
    for (const auto& child : nodes) {
        child->accept(*this);
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::DeclStmtAST& node) {
    const auto& varDefs = node.getVarDefs();
    for (const auto& varDef : varDefs) {
        varDef->accept(*this);
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::VarDefAST& node) {
    const std::string& symName = node.getName(); // 获取符号名
    llvm::Type* baseType = getLLVMType(node.getType()); // 获取符号基本类型
    
    // 为变量分配内存
    llvm::Value* symAddr = nullptr;

    if(symbolTable.isGlobalScope()) { // 全局作用域
        if(!node.isArray()) { // 定义全局普通变量
            llvm::Constant* initVal = nullptr;

            if(node.isInited()) { // 如果全局变量有初值表达式
                AST::float32 constInitVal = node.getConstInitVal();
                initVal = baseType->isIntegerTy(32) ? 
                    llvm::ConstantInt::get(intType, llvm::APInt(32, static_cast<AST::int32>(constInitVal), true)) : 
                    llvm::ConstantFP::get(floatType, llvm::APFloat(constInitVal));
            } else {
                initVal = baseType->isIntegerTy(32) ? zeroInt : zeroFloat;
            }

            symAddr = new llvm::GlobalVariable( // 创建全局变量
                *module,
                baseType,
                node.isConst(),
                llvm::GlobalValue::ExternalLinkage,
                initVal, // 全局变量的默认值为0或0.0f
                symName);

            // 记录变量信息
            symbolTable.setNamedValue(symName, SymbolInfo(SymbolKind::GLOBAL_VARIABLE, baseType, symAddr));

        } else { // 定义全局数组
            const auto& arrDims = node.getArrayDims(); // 数组维度
            AST::int32 totalValNum = std::accumulate(arrDims.begin(), arrDims.end(), 1, std::multiplies<AST::int32>()); // 计算数组元素总数
            llvm::Type* arrayType = getLLVMArrayType(baseType, arrDims); // 获取数组类型
            // 创建全局变量的时候，直接把类型改为一维数组，这样扁平初始化就合法了
            // 在访问的时候用arrayType访问
            llvm::Type* flattenArrayType = getLLVMArrayType(baseType, {totalValNum});
            llvm::Constant* initValList = nullptr;

            if(node.isInited()) { // 全局数组、有数组初始化器
                const auto& constInitValList = node.getConstInitValList();
                if(constInitValList.size() != totalValNum) {
                    
                    /****************************************************************************/
                    std::cout << "1. constInitValList.size() = " << constInitValList.size() << std::endl;
                    std::cout << "1. totalValNum = " << totalValNum << std::endl;
                    /****************************************************************************/

                    throw std::logic_error("Array initializer size does not match array size.\n");
                }
                std::vector<llvm::Constant*> elems;
                for(const auto& val : constInitValList) {
                    llvm::Constant* temp = baseType->isIntegerTy(32) ? 
                                llvm::ConstantInt::get(intType,llvm::APInt(32, static_cast<AST::int32>(val), true)) : 
                                llvm::ConstantFP::get(floatType, llvm::APFloat(val));
                    elems.push_back(temp);
                }

                initValList = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(arrayType), elems);
            } else { // 全局数组、没有数组初始化器
                initValList = llvm::ConstantAggregateZero::get(arrayType); // 数组零初始化
            }

            symAddr = new llvm::GlobalVariable( // 创建全局数组
                *module,
                flattenArrayType,
                node.isConst(),
                llvm::GlobalValue::ExternalLinkage,
                initValList, // 数组的默认值为0或0.0f
                symName);
                
            // 记录变量信息
            symbolTable.setNamedValue(symName, SymbolInfo(SymbolKind::GLOBAL_ARRAY, arrayType, symAddr));
        }

    } else { // 局部作用域
        if(!node.isArray()) { // 定义局部普通变量
            symAddr = createAlloca(baseType, nullptr);
            // 记录变量信息
            symbolTable.setNamedValue(symName, SymbolInfo(SymbolKind::LOCAL_VARIABLE, baseType, symAddr));

            if(node.isInited()) { // 如果局部变量有初值表达式
                node.getInitVal()->accept(*this);
                if (!currentValue) {
                    throw std::logic_error("Local variable initializer is null.\n");
                }
                currentValue = createCast(currentValue, baseType);
                builder->CreateStore(currentValue, symAddr);
            }
        } else { // 定义局部数组
            const auto& arrDims = node.getArrayDims(); // 数组维度
            AST::int32 totalValNum = std::accumulate(arrDims.begin(), arrDims.end(), 1, std::multiplies<AST::int32>()); // 计算数组元素总数
            llvm::Type* arrayType = getLLVMArrayType(baseType, arrDims); // 获取数组类型
            symAddr = createAlloca(arrayType, nullptr);
            // 记录变量信息
            symbolTable.setNamedValue(symName, SymbolInfo(SymbolKind::LOCAL_ARRAY, arrayType, symAddr));

            if(node.isInited()) {
                std::vector<llvm::Value*> elems;
                flattenList(baseType, arrDims, node.getInitVal().get(), elems);
                if(elems.size() != totalValNum) {

                    /****************************************************************************/
                    std::cout << "2. elems.size() = " << elems.size() << std::endl;
                    std::cout << "2. totalValNum = " << totalValNum << std::endl;
                    /****************************************************************************/

                    throw std::logic_error("Array initializer size does not match array size.\n");
                }

                for(AST::int32 i = 0; i< totalValNum; i++) {
                    llvm::Constant* index = llvm::ConstantInt::get(intType, llvm::APInt(32, i, true));
                    auto* targetPtr = builder->CreateGEP(baseType, symAddr, {index});
                    builder->CreateStore(elems[i], targetPtr);
                }
            }
        }
    }
    // 添加变量调试信息
    // debugInfoGenerator->createVariableDebugInfo(name, node.getType(), VarAddr);
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::InitValAST& node) {
    if (node.isList()) {
        throw std::logic_error("Array initializer or expression is not supported.\n");
    }
    node.getExp()->accept(*this);
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::ArraySubscriptAST& node) {
    node.getExp()->accept(*this);
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::FuncDefAST& node) {
    const std::string& funcName = node.getName(); // 获取函数名
    llvm::Type* returnType = getLLVMType(node.getType());
    currentFuncReturnType = node.getType(); // 获取函数返回类型
    
    std::vector<llvm::Type*> llvmParamTypes; // 函数参数类型
    if (!node.isEmptyParam()) { // 如果函数有参数
        const auto& params = node.getParams();
        for (const auto& param : params) {
            llvm::Type* baseType = getLLVMType(param->getType());
            if(!param->isArray()) { // 普通参数
                llvmParamTypes.push_back(baseType);
            } else { // 数组参数
                llvmParamTypes.push_back(ptrType);
            }
        }
    }
    
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        llvmParamTypes,
        false
    );
    
    llvm::Function* function = llvm::Function::Create(
        funcType,
        llvm::Function::ExternalLinkage,
        funcName,
        module.get()
    );
    
    // // 收集参数类型
    // std::vector<BType> paramTypes;
    // if (!node.isEmptyParam()) {
    //     const auto& params = node.getParams();
    //     for (const auto& param : params) {
    //         paramTypes.push_back(param->getType());
    //     }
    // }
    
    // // 添加函数调试信息
    // llvm::DISubprogram* subprogram = debugInfoGenerator->createFunctionDebugInfo(
    //     funcName,
    //     funcType,
    //     function,
    //     paramTypes,
    //     node.getType()
    // );
    
    symbolTable.setFunction(funcName, function);
    
    llvm::BasicBlock* entryBlock = llvm::BasicBlock::Create(*context, "entry", function); // 入口块
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(*context, "body", function); // 主体块
    
    builder->SetInsertPoint(entryBlock); // 设置插入点为入口块
    llvm::BranchInst* brInst = builder->CreateBr(bodyBlock); // 跳转到主体块
    
    allocaInsertBlock = entryBlock; // 分配指令插入点设置为入口块
    allocaInsertPoint = brInst->getIterator(); // 分配指令插入点设置为跳转指令前的位置
    
    builder->SetInsertPoint(bodyBlock); // 设置插入点为主体块
    
    // // 设置调试信息作用域
    // debugInfoGenerator->setDebugLocation(0, 0);
    
    symbolTable.enterScope(); // 进入函数作用域
    symbolTable.setMergeScope(true);
    
    // 为函数参数创建分配指令
    if (!node.isEmptyParam()) {
        const auto& params = node.getParams();
        size_t idx = 0;
        for (auto& arg : function->args()) {
            arg.setName(params[idx]->getName());
            llvm::AllocaInst* alloca = createAlloca(arg.getType(), nullptr);
            builder->CreateStore(&arg, alloca);

            llvm::Type* baseType = getLLVMType(params[idx]->getType());
            llvm::Type* symType = arg.getType();
            if(symType->isPointerTy()) {
                const auto& arrDims = params[idx]->getArrayDims();
                llvm::Type* arrayType = getLLVMArrayType(baseType, std::vector<AST::int32>(arrDims.begin() + 1, arrDims.end()));
                symbolTable.setNamedValue(params[idx]->getName(), SymbolInfo(SymbolKind::PARAMETER_PTR, arrayType, alloca));
            } else {
                symbolTable.setNamedValue(params[idx]->getName(), SymbolInfo(SymbolKind::PARAMETER_VARIABLE, symType, alloca));
            }
            idx++;
        }
    }
    
    if (node.getBody()) { // 若函数有主体
        node.getBody()->accept(*this); // 生成函数主体的IR
    }
    
    // 若主体块没有终止指令，根据函数返回类型添加默认返回值
    if (!builder->GetInsertBlock()->getTerminator()) {
        if (node.getType() == AST::BType::VOID) {
            builder->CreateRetVoid();
        } else if (node.getType() == AST::BType::INT) {
            builder->CreateRet(zeroInt);
        } else if (node.getType() == AST::BType::FLOAT) {
            builder->CreateRet(zeroFloat);
        }
    }
    
    // 这里不需要exitScope，因为BlockStmtAST中已经exitScope了
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::FuncFParamAST& node) {
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::DeclRefAST& node) {
    const std::string& symName = node.getName(); // 获取符号名（因为有可能是普通变量variable，也有可能是数组array）
    SymbolInfo* symInfo = symbolTable.getNamedValue(symName); // 获取符号信息

    if (!symInfo) { 
        // 变量未定义（其实不存在这种情况，否则在语义分析阶段就会报错）
        // 这里报错是为了防止编写的代码有逻辑错误
        throw std::logic_error("Variable not found: " + symName);
    }

    llvm::Value* symAddr = symInfo->address; // 获取符号地址
    
    if (!symAddr) {
        // 另号地址为空（其实不存在这种情况）
        // 这里报错是为了防止编写的代码有逻辑错误
        throw std::logic_error("Symbol address is null: " + symName);
    }
    
    SymbolKind symKind = symInfo->kind;
    llvm::Type* symType = symInfo->type;


    if(symKind == SymbolKind::GLOBAL_VARIABLE 
        || symKind == SymbolKind::LOCAL_VARIABLE 
        || symKind == SymbolKind::PARAMETER_VARIABLE) {
        currentValue = builder->CreateLoad(symType, symAddr);
    } else if(symKind == SymbolKind::GLOBAL_ARRAY || symKind == SymbolKind::LOCAL_ARRAY) {
        currentValue = builder->CreateGEP(symType, symAddr, {zeroInt, zeroInt});
    } else if(symKind == SymbolKind::PARAMETER_PTR) {
        currentValue = builder->CreateLoad(ptrType, symAddr);
    }

}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::ArrayExprAST& node) { 
    llvm::Type* baseType = getLLVMType(node.getEType().getType());
    const std::string& name = node.getName(); // 获取数组名
    SymbolInfo* symInfo = symbolTable.getNamedValue(name); // 获取数组符号信息
    if (!symInfo) { // 数组未定义（其实不存在这种情况，否则在语义分析阶段就会报错）
        throw std::logic_error("Array not found: " + name);
    }
    
    llvm::Value* arrayPtr = symInfo->address; // 获取数组指针
    
    if (!arrayPtr) { // 数组指针为空（其实不存在这种情况）
        throw std::logic_error("Array address is null: " + name);
    }
    
    SymbolKind symKind = symInfo->kind;
    llvm::Type* symType = symInfo->type;

    std::vector<llvm::Value*> idxList;

    if(symKind == SymbolKind::PARAMETER_PTR) {
        arrayPtr = builder->CreateLoad(ptrType, arrayPtr);
    }

    if(symKind == SymbolKind::GLOBAL_ARRAY || symKind == SymbolKind::LOCAL_ARRAY) {
        idxList.push_back(zeroInt);
    }

    const auto& indices = node.getIndices();
    for (const auto& index : indices) {
        index->accept(*this);
        if (!currentValue) {
            throw std::logic_error("Array index is invalid: " + name);
        }
        idxList.push_back(currentValue);
    }
    
    auto* targetPtr = builder->CreateGEP(
        symType,
        arrayPtr,
        idxList
    );

    if(node.getEType().isNumeric()) { // 数组表达式定位到元素
        currentValue = builder->CreateLoad(baseType, targetPtr);
    } else if(node.getEType().isArray()) {
        currentValue = builder->CreateGEP(
            symType,
            targetPtr,
            {zeroInt, zeroInt}
        );
    }
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::CallExprAST& node) {
    const std::string& funcName = node.getName();
    FunctionInfo* calleeFuncInfo = symbolTable.getFunction(funcName);
    
    if (!calleeFuncInfo) {
        throw std::logic_error("Function not found: " + funcName);
    }

    llvm::Function* callee = calleeFuncInfo->function;
    
    std::vector<llvm::Value*> args;
    if (!node.isEmptyParam()) {
        const auto& params = node.getParams();
        size_t idx = 0;
        for (const auto& param : params) {
            param->accept(*this);
            if (!currentValue) {
                throw std::logic_error("Parameter is invalid: " + funcName);
            }

            // 按照被调函数的形参类型做转换
            if (idx < callee->arg_size()) {
                llvm::Type* expectedType = callee->getFunctionType()->getParamType(idx);
                // 只对基本类型（int/float/bool）做转换，指针类型不处理
                if (expectedType->isIntegerTy(32) || expectedType->isFloatTy() || expectedType->isIntegerTy(1)) {
                    currentValue = createCast(currentValue, expectedType);
                }
            }
            args.push_back(currentValue);
            idx++;
        }
    }
    
    // lli: lli: a.out:46:3: error: instructions returning void cannot have a name
    // %func.call = call void @func(i32 %arg1.load1, ptr %arg2.load2)
    // ^
    // currentValue = builder->CreateCall(callee, args, funcName + ".call");

    currentValue = builder->CreateCall(callee, args);
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::FuncRParamAST& node) {
    node.getExp()->accept(*this);
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::BinaryExprAST& node) {
    AST::BinaryOpType op = node.getBinaryOpType();
    
    if (op == AST::BinaryOpType::ASSIGN) {
        llvm::Type* baseType = getLLVMType(node.getLhs().get()->getEType().getType());
        llvm::Value* lhsAddr = nullptr;
        if (auto declRef = llvm::dyn_cast<AST::DeclRefAST>(node.getLhs().get())) { // 左操作数是普通变量引用（当LVal表示数组时，方括号个数必须和数组变量的维数相同（即定位到元素），所以DeclRefAST不可能引用了数组变量）
            lhsAddr = symbolTable.getNamedValue(declRef->getName())->address;
        } else if (auto arrayExpr = llvm::dyn_cast<AST::ArrayExprAST>(node.getLhs().get())) { // 当LVal表示数组时，方括号个数必须和数组变量的维数相同（即定位到元素）
            const std::string& arrayName = arrayExpr->getName(); // 获取数组名
            llvm::Type* baseType = getLLVMType(arrayExpr->getEType().getType());
            SymbolInfo* symInfo = symbolTable.getNamedValue(arrayName); // 获取数组符号信息
            SymbolKind arrayKind = symInfo->kind; // 获取数组符号类型
            llvm::Type* arrayType = symInfo->type; // 获取数组类型
            lhsAddr = symInfo->address; // 获取数组指针
            
            std::vector<llvm::Value*> idxList;

            if(arrayKind == SymbolKind::PARAMETER_PTR) {
                lhsAddr = builder->CreateLoad(ptrType, lhsAddr);
            }

            if(arrayKind == SymbolKind::GLOBAL_ARRAY || arrayKind == SymbolKind::LOCAL_ARRAY) {
                idxList.push_back(zeroInt);
            }

            const auto& indices = arrayExpr->getIndices();
            for (const auto& index : indices) {
                index->accept(*this);
                if (!currentValue) {
                    throw std::logic_error("Array index is invalid: " + arrayName);
                }
                idxList.push_back(currentValue);
            }

            lhsAddr = builder->CreateGEP(
                arrayType,
                lhsAddr,
                idxList
            );
        }

        if (!lhsAddr) {
            throw std::logic_error("Left-hand side of assignment is invalid");
        }
        
        node.getRhs()->accept(*this);
        llvm::Value* rhsValue = currentValue;
        if (!rhsValue) {
            throw std::logic_error("Right-hand side of assignment is invalid");
        }

        rhsValue = createCast(rhsValue, baseType);

        builder->CreateStore(rhsValue, lhsAddr);
        currentValue = rhsValue;

        return;
    }

    if(op == AST::BinaryOpType::AND) {
        // 短路求值：AND 操作
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* rhsBlock = llvm::BasicBlock::Create(*context, "and.rhs", function);
        llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, "and.end", function);

        node.getLhs()->accept(*this);
        llvm::Value* lhs = convertToBool(currentValue);
        llvm::BasicBlock* lhsBlock = builder->GetInsertBlock();
        builder->CreateCondBr(lhs, rhsBlock, endBlock);

        // 右操作数块
        builder->SetInsertPoint(rhsBlock);
        node.getRhs()->accept(*this);
        llvm::Value* rhsVal = convertToBool(currentValue);
        llvm::BasicBlock* rhsEndBlock = builder->GetInsertBlock();
        builder->CreateBr(endBlock);

        // 结束块
        builder->SetInsertPoint(endBlock);
        llvm::PHINode* phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "and.tmp");
        phi->addIncoming(llvm::ConstantInt::getFalse(*context), lhsBlock);
        phi->addIncoming(rhsVal, rhsEndBlock);
        currentValue = phi;
        
        return;
    } else if(op == AST::BinaryOpType::OR) {
        // 短路求值：OR 操作
        llvm::Function* function = builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* rhsBlock = llvm::BasicBlock::Create(*context, "or.rhs", function);
        llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, "or.end", function);

        node.getLhs()->accept(*this);
        llvm::Value* lhs = convertToBool(currentValue);
        llvm::BasicBlock* lhsBlock = builder->GetInsertBlock();
        builder->CreateCondBr(lhs, endBlock, rhsBlock);

        // 右操作数块
        builder->SetInsertPoint(rhsBlock);
        node.getRhs()->accept(*this);
        llvm::Value* rhsVal = convertToBool(currentValue);
        llvm::BasicBlock* rhsEndBlock = builder->GetInsertBlock();
        builder->CreateBr(endBlock);

        // 结束块
        builder->SetInsertPoint(endBlock);
        llvm::PHINode* phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "or.tmp");
        phi->addIncoming(llvm::ConstantInt::getTrue(*context), lhsBlock);
        phi->addIncoming(rhsVal, rhsEndBlock);
        currentValue = phi;
        
        return;
    }


    node.getLhs()->accept(*this);
    llvm::Value* lhs = currentValue;
    node.getRhs()->accept(*this);
    llvm::Value* rhs = currentValue;
    
    if (!lhs || !rhs) {
        throw std::logic_error("Binary expression operands are invalid");
    }
    
    if (lhs->getType()->isFloatTy() || rhs->getType()->isFloatTy()) {
        if (lhs->getType()->isIntegerTy()) {
            lhs = convertToFloat(lhs);
        }
        if (rhs->getType()->isIntegerTy()) {
            rhs = convertToFloat(rhs);
        }
    }

    if(lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy()) {
        if(lhs->getType()->isIntegerTy(1)) {
            lhs = convertToInt(lhs);
        }
        if(rhs->getType()->isIntegerTy(1)) {
            rhs = convertToInt(rhs);
        }
    }
    
    bool isFloat = lhs->getType()->isFloatTy();
    
    switch (op) {
        case AST::BinaryOpType::ADD:
            currentValue = isFloat ? builder->CreateFAdd(lhs, rhs, "addtmp") : builder->CreateAdd(lhs, rhs, "addtmp");
            break;
        case AST::BinaryOpType::SUB:
            currentValue = isFloat ? builder->CreateFSub(lhs, rhs, "subtmp") : builder->CreateSub(lhs, rhs, "subtmp");
            break;
        case AST::BinaryOpType::MUL:
            currentValue = isFloat ? builder->CreateFMul(lhs, rhs, "multmp") : builder->CreateMul(lhs, rhs, "multmp");
            break;
        case AST::BinaryOpType::DIV:
            currentValue = isFloat ? builder->CreateFDiv(lhs, rhs, "divtmp") : builder->CreateSDiv(lhs, rhs, "divtmp");
            break;
        case AST::BinaryOpType::MOD:
            currentValue = builder->CreateSRem(lhs, rhs, "modtmp");
            break;
        case AST::BinaryOpType::LT:
            currentValue = isFloat ? builder->CreateFCmpOLT(lhs, rhs, "lttmp") : builder->CreateICmpSLT(lhs, rhs, "lttmp");
            break;
        case AST::BinaryOpType::GT:
            currentValue = isFloat ? builder->CreateFCmpOGT(lhs, rhs, "gttmp") : builder->CreateICmpSGT(lhs, rhs, "gttmp");
            break;
        case AST::BinaryOpType::LE:
            currentValue = isFloat ? builder->CreateFCmpOLE(lhs, rhs, "letmp") : builder->CreateICmpSLE(lhs, rhs, "letmp");
            break;
        case AST::BinaryOpType::GE:
            currentValue = isFloat ? builder->CreateFCmpOGE(lhs, rhs, "getmp") : builder->CreateICmpSGE(lhs, rhs, "getmp");
            break;
        case AST::BinaryOpType::EQ:
            currentValue = isFloat ? builder->CreateFCmpOEQ(lhs, rhs, "eqtmp") : builder->CreateICmpEQ(lhs, rhs, "eqtmp");
            break;
        case AST::BinaryOpType::NE:
            currentValue = isFloat ? builder->CreateFCmpONE(lhs, rhs, "netmp") : builder->CreateICmpNE(lhs, rhs, "netmp");
            break;
        default:
            currentValue = nullptr;
    }
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::UnaryExprAST& node) {
    node.getExp()->accept(*this);
    llvm::Value* operand = currentValue;
    
    if (!operand) {
        currentValue = nullptr;
        return;
    }
    
    switch (node.getUnaryOpType()) {
        case AST::UnaryOpType::PLUS:
            currentValue = operand;
            break;
        case AST::UnaryOpType::MINUS: {
            llvm::Type* type = operand->getType();
            if (type->isFloatTy()) {
                currentValue = builder->CreateFNeg(operand, "negtmp");
            } else if (type->isIntegerTy(32)){
                currentValue = builder->CreateNeg(operand, "negtmp");
            } else if (type->isIntegerTy(1)){
                operand = convertToInt(operand);
                currentValue = builder->CreateNeg(operand, "negtmp");
            }
            break;
        }
        case AST::UnaryOpType::NOT:
            operand = convertToBool(operand);
            currentValue = builder->CreateXor(operand, llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), 1), "nottmp");
            break;
    }
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::IntLiteralAST& node) {
    currentValue = llvm::ConstantInt::get(intType, llvm::APInt(32, node.getValue(), true));
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::FloatLiteralAST& node) {
    currentValue = llvm::ConstantFP::get(floatType, llvm::APFloat(node.getValue()));
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::BlockStmtAST& node) {
    symbolTable.enterScope();
    const auto& items = node.getItems();
    for (const auto& item : items) {
        item->accept(*this);
    }
    symbolTable.exitScope();
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::IfStmtAST& node) {
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(*context, "then", function);
    llvm::BasicBlock* elseBlock = node.hasElse() ? llvm::BasicBlock::Create(*context, "else", function) : nullptr;
    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(*context, "ifcont", function);
    
    node.getCond()->accept(*this);
    llvm::Value* condValue = convertToBool(currentValue);
    
    if (node.hasElse()) {
        builder->CreateCondBr(condValue, thenBlock, elseBlock);
    } else {
        builder->CreateCondBr(condValue, thenBlock, mergeBlock);
    }
    
    builder->SetInsertPoint(thenBlock);
    node.getThenBody()->accept(*this);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(mergeBlock);
    }
    
    if (node.hasElse()) {
        builder->SetInsertPoint(elseBlock);
        node.getElseBody()->accept(*this);
        if (!builder->GetInsertBlock()->getTerminator()) {
            builder->CreateBr(mergeBlock);
        }
    }
    
    builder->SetInsertPoint(mergeBlock);
}
/*******************************************************************************************/   
void IRGenVisitor::visit(AST::WhileStmtAST& node) {
    llvm::Function* function = builder->GetInsertBlock()->getParent();
    
    llvm::BasicBlock* condBlock = llvm::BasicBlock::Create(*context, "whilecond", function);
    llvm::BasicBlock* bodyBlock = llvm::BasicBlock::Create(*context, "whilebody", function);
    llvm::BasicBlock* endBlock = llvm::BasicBlock::Create(*context, "whileend", function);
    
    loopStack.push({condBlock, endBlock});
    
    builder->CreateBr(condBlock);
    
    builder->SetInsertPoint(condBlock);
    node.getCondition()->accept(*this);
    llvm::Value* condValue = convertToBool(currentValue);
    builder->CreateCondBr(condValue, bodyBlock, endBlock);
    
    builder->SetInsertPoint(bodyBlock);
    node.getBody()->accept(*this);
    if (!builder->GetInsertBlock()->getTerminator()) {
        builder->CreateBr(condBlock);
    }
    
    builder->SetInsertPoint(endBlock);
    
    loopStack.pop();
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::ReturnStmtAST& node) {
    if (node.hasReturnExpr()) {
        node.getReturnExpr()->accept(*this);
        llvm::Value* retValue = currentValue;
        if (!retValue) {
            throw std::logic_error("Return statement must have a value");
        }
        llvm::Type* retType = getLLVMType(currentFuncReturnType);
        retValue = createCast(retValue, retType);
        builder->CreateRet(retValue);
    } else {
        builder->CreateRetVoid();
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::BreakStmtAST& node) {
    if (!loopStack.empty()) {
        builder->CreateBr(loopStack.top().endBlock);
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::ContinueStmtAST& node) {
    if (!loopStack.empty()) {
        builder->CreateBr(loopStack.top().condBlock);
    }
}
/*******************************************************************************************/
void IRGenVisitor::visit(AST::NullStmtAST& node) {
}
/*******************************************************************************************/
}  // namespace IRGen
