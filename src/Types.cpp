#include "Types.hpp"

namespace AST {
//------------------------------------------------------------------------
// 将 BType 转换为字符串
std::string typeToString(BType type) {
    switch (type) {
        case BType::INT: return "int";
        case BType::FLOAT: return "float";
        case BType::VOID: return "void";
        default: return "undef";
    }
}
//------------------------------------------------------------------------
// 将 UnaryOpType 转换为字符串
std::string unaryOpToString(UnaryOpType op) {
    switch (op) {
        case UnaryOpType::PLUS: return "+";
        case UnaryOpType::MINUS: return "-";
        case UnaryOpType::NOT: return "!";
        default: return "op";
    }
}
//------------------------------------------------------------------------
// 将 BinaryOpType 转换为字符串
std::string binaryOpToString(BinaryOpType op) {
    switch (op) {
        case BinaryOpType::ASSIGN: return "=";
        case BinaryOpType::ADD: return "+";
        case BinaryOpType::SUB: return "-";
        case BinaryOpType::MUL: return "*";
        case BinaryOpType::DIV: return "/";
        case BinaryOpType::MOD: return "%";
        case BinaryOpType::LT: return "<";
        case BinaryOpType::GT: return ">";
        case BinaryOpType::LE: return "<=";
        case BinaryOpType::GE: return ">=";
        case BinaryOpType::EQ: return "==";
        case BinaryOpType::NE: return "!=";
        case BinaryOpType::AND: return "&&";
        case BinaryOpType::OR: return "||";
        default: return "op";
    }
}
//------------------------------------------------------------------------

} // namespace AST