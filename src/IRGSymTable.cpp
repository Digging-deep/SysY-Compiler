#include "IRGSymTable.hpp"

namespace IRGen {

IRGSymTable::IRGSymTable() {
    enterScope();
}

void IRGSymTable::enterScope() {
    if(!isMergeScope()) {
        namedValues.push_back(std::unordered_map<std::string, SymbolInfo>());
    }
    mergeScope = false;
}

void IRGSymTable::exitScope() {
    if (namedValues.size() <= 1)
        throw std::logic_error("IRGSymTable::exitScope: exitScope on global scope");
    namedValues.pop_back();
}

void IRGSymTable::setNamedValue(const std::string& name, SymbolInfo info) {
    namedValues.back().emplace(name, info);
}

SymbolInfo* IRGSymTable::getNamedValue(const std::string& name) {
    for (auto it = namedValues.rbegin(); it != namedValues.rend(); ++it) {
        auto valIt = it->find(name);
        if (valIt != it->end()) {
            return &valIt->second;
        }
    }
    return nullptr;
}

void IRGSymTable::setFunction(const std::string& name, FunctionInfo info) {
    functions.emplace(name, info);
}

FunctionInfo* IRGSymTable::getFunction(const std::string& name) {
    auto it = functions.find(name);
    if (it != functions.end()) {
        return &it->second;
    }
    return nullptr;
}

bool IRGSymTable::hasFunction(const std::string& name) const {
    return functions.find(name) != functions.end();
}

bool IRGSymTable::isGlobalScope() const {
    return namedValues.size() == 1;
}


} // namespace IRGen
