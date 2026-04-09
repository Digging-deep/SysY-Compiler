#include "SCSymTable.hpp"

namespace Sema {

SCSymTable::SCSymTable() {
    // 初始化全局作用域
    enterScope();
}

void SCSymTable::enterScope() {
    // 如果当前合并作用域，不创建新作用域
    if (!isMergeScope()) {
        scopeStack.push_back(Scope());
    }
    mergeScope = false;
}

void SCSymTable::exitScope() {
    if (scopeStack.size() <= 1)
        throw std::logic_error("SCSymTable::exitScope: exitScope on global scope");
    scopeStack.pop_back();
}

bool SCSymTable::insertVar(const std::string& name, const SymbolInfo& info) {
    if (info.kind == SymbolKind::FUNCTION)
        throw std::logic_error("insertVar: function symbol is not allowed");
    if(scopeStack.empty()) 
        throw std::logic_error("insertVar: scope stack is empty");

    Scope& currentScope = scopeStack.back();
    if (currentScope.symbols.find(name) != currentScope.symbols.end()) {
        return false;
    }
    currentScope.symbols[name] = info;
    return true;
}

bool SCSymTable::insertVarInGlobalScope(const std::string& name, const SymbolInfo& info) {
    if (info.kind == SymbolKind::FUNCTION)
        throw std::logic_error("insertVarInGlobalScope: function symbol is not allowed");
    if(scopeStack.empty()) 
        throw std::logic_error("insertVarInGlobalScope: scope stack is empty");

    Scope& globalScope = scopeStack[0];
    if (globalScope.symbols.find(name) != globalScope.symbols.end()) {
        return false;
    }
    globalScope.symbols[name] = info;
    return true;
}

SymbolInfo* SCSymTable::lookupVar(const std::string& name) {
    if(scopeStack.empty()) 
        throw std::logic_error("lookupVar: scope stack is empty");

    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto symbolIt = it->symbols.find(name);
        if (symbolIt != it->symbols.end()) {
            return &symbolIt->second;
        }
    }
    return nullptr;
}

SymbolInfo* SCSymTable::lookupVarInCurrentScope(const std::string& name) {
    if(scopeStack.empty()) 
        throw std::logic_error("lookupVarInCurrentScope: scope stack is empty");

    Scope& currentScope = scopeStack.back();
    auto it = currentScope.symbols.find(name);
    if (it != currentScope.symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

SymbolInfo* SCSymTable::lookupVarInGlobalScope(const std::string& name) {
    if(scopeStack.empty()) 
        throw std::logic_error("lookupVarInGlobalScope: scope stack is empty");

    Scope& globalScope = scopeStack[0];
    auto it = globalScope.symbols.find(name);
    if (it != globalScope.symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

bool SCSymTable::insertFunc(const std::string& name, const SymbolInfo& info) {
    if (info.kind != SymbolKind::FUNCTION)
        throw std::logic_error("insertFunc: symbol is not a function");
    
    if (functionScope.symbols.find(name) != functionScope.symbols.end()) 
        return false;
    
    functionScope.symbols[name] = info;
    return true;
}

SymbolInfo* SCSymTable::lookupFunc(const std::string& name) {
    auto functionIt = functionScope.symbols.find(name);
    if (functionIt != functionScope.symbols.end()) {
        return &functionIt->second;
    }
    return nullptr;
}


} // namespace Sema
