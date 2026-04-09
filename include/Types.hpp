#ifndef SYSY2022_TYPES_HPP
#define SYSY2022_TYPES_HPP
#include <cstdint>
#include <string>
#include <vector>

namespace AST {
//-------------------------------------------------------------------------------------------------
// Type alias
using int32 = int32_t;
using float32 = float;
using string = std::string;
//-------------------------------------------------------------------------------------------------
// Base type
enum class BType{
    INT,
    FLOAT,
    VOID,
    UNDEFINED
};
//-------------------------------------------------------------------------------------------------
// Extended type, just for SemaCheckVisitor
//
// Example:
// int a[2][3]
// the EType of a[1] is int[3]:
//      type = BType::INT;
//      dims = {3};
class EType{
private:
    BType type;
    std::vector<int> dims;

public:
    EType() : type(BType::UNDEFINED), dims() {}
    EType(BType type, const std::vector<int>& dims = {})
        : type(type), dims(dims) {}

    BType getType() const { return type; }
    const std::vector<int>& getDims() const { return dims; }

    bool isInt() const { return type == BType::INT && dims.empty(); }
    bool isFloat() const { return type == BType::FLOAT && dims.empty(); }
    bool isNumeric() const { return isInt() || isFloat(); }
    bool isFloatArray() const { return isFloat() && isArray(); }
    bool isIntArray() const { return isInt() && isArray(); }
    bool isArray() const { return !dims.empty() && type != BType::VOID && type != BType::UNDEFINED; }
    bool isVoid() const { return type == BType::VOID && dims.empty(); }
    bool isUndefined() const { return type == BType::UNDEFINED && dims.empty(); }
    void setType(BType type) { this->type = type; }
    void setDims(const std::vector<int>& dims) { this->dims = dims; }
    void addDim(int dim) { this->dims.push_back(dim); }
};
//-------------------------------------------------------------------------------------------------
// Unary operator type
// Note: '!' only appears in conditional expressions.
enum class UnaryOpType {
    PLUS,   // positive
    MINUS,  // negative
    NOT     // logical not
};
//-------------------------------------------------------------------------------------------------
// Binary operator type
enum class BinaryOpType {
    ASSIGN, // =
    ADD,    // +
    SUB,    // -
    MUL,    // *
    DIV,    // /
    MOD,    // %
    LT,     // <
    GT,     // >
    LE,     // <=
    GE,     // >=
    EQ,     // ==
    NE,    // !=
    AND,    // &&
    OR      // ||
};
//-------------------------------------------------------------------------------------------------
std::string typeToString(BType type);
std::string unaryOpToString(UnaryOpType op);
std::string binaryOpToString(BinaryOpType op);
//-------------------------------------------------------------------------------------------------
}// namespace AST
#endif // SYSY2022_TYPES_HPP