%option noyywrap nodefault nounput noinput nomain
%option yylineno

%{
#include <string>   // just for std::stoi
#include <cstdint>  // just for int32_t
#include <stdexcept> // for exception handling
#include "parser.tab.hpp"
#include "llvm/Support/raw_ostream.h" // just for llvm::errs()
#define YY_DECL yy::parser::symbol_type yylex ()

# define YY_USER_ACTION  loc.columns(yyleng);

bool isOverflow = false;
%}
%x COMMENT

%option header-file="lexer.hpp"
%option outfile="lexer.cpp"

%%
%{
    // 位置追踪
    static yy::location loc;
    // 每次yylex被调用，这里都会执行一次，把begin移动到end的位置（即"确认上一个token已消费"）
    loc.step();
%}

    /* 识别整数，分别识别八进制数字串、十六进制数字串和十进制数字串 */
0[0-7]+             { 
                        try {
                            return yy::parser::make_INTCONST((int32_t)std::stoi(yytext, nullptr,  8), loc); 
                        } catch (const std::out_of_range& e) {
                            llvm::errs() << "error" << "(" << loc.begin.line << "," << loc.begin.column << "): ";
                            llvm::errs() << "semantic error, integer overflow" << "\n";
                            isOverflow = true;
                            // 继续分析，返回一个默认值
                            return yy::parser::make_INTCONST(0, loc);
                        }
                      }
0[xX][0-9a-fA-F]+   { 
                        try {
                            return yy::parser::make_INTCONST((int32_t)std::stoi(yytext, nullptr, 16), loc); 
                        } catch (const std::out_of_range& e) {
                            llvm::errs() << "error" << "(" << loc.begin.line << "," << loc.begin.column << "): ";
                            llvm::errs() << "semantic error, integer overflow\n";
                            isOverflow = true;
                            // 继续分析，返回一个默认值
                            return yy::parser::make_INTCONST(0, loc);
                        }
                      }
0|([1-9][0-9]*)     { 
                        try {
                            return yy::parser::make_INTCONST((int32_t)std::stoi(yytext, nullptr, 10), loc); 
                        } catch (const std::out_of_range& e) {
                            llvm::errs() << "error" << "(" << loc.begin.line << "," << loc.begin.column << "): ";
                            llvm::errs() << "semantic error, integer overflow\n";
                            isOverflow = true;
                            // 继续分析，返回一个默认值
                            return yy::parser::make_INTCONST(0, loc);
                        }
                      }

    /* 识别浮点数 */
(([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+))([eE][\+-]?[0-9]+)? |
[0-9]+[eE][\+-]?[0-9]+ |
0[xX](([0-9a-fA-F]+)|([0-9a-fA-F]+\.[0-9a-fA-F]*)|([0-9a-fA-F]*\.[0-9a-fA-F]+))[pP][\+-]?[0-9]+ { 
                                                                                                    try {
                                                                                                            return yy::parser::make_FLOATCONST((float)std::stof(yytext), loc); 
                                                                                                    } catch (const std::out_of_range& e) {
                                                                                                        llvm::errs() << "error" << "(" << loc.begin.line << "," << loc.begin.column << "): ";
                                                                                                        llvm::errs() << "semantic error, float overflow\n";
                                                                                                        isOverflow = true;
                                                                                                        // 继续分析，返回一个默认值
                                                                                                        return yy::parser::make_FLOATCONST(0.0f, loc);
                                                                                                    }
                                                                                                }

    /*单一字符操作符*/
"="     { return yy::parser::make_ASSIGN(loc);    }
","     { return yy::parser::make_COMMA(loc);     }
";"     { return yy::parser::make_SEMICOLON(loc); }
"("     { return yy::parser::make_LPAR(loc);      }
")"     { return yy::parser::make_RPAR(loc);      }
"["     { return yy::parser::make_LSQUARE(loc);   }
"]"     { return yy::parser::make_RSQUARE(loc);   }
"{"     { return yy::parser::make_LBRACKET(loc);  }
"}"     { return yy::parser::make_RBRACKET(loc);  }

    /*基本的算术运算*/
"+"     { return yy::parser::make_ADD(loc);    }
"-"     { return yy::parser::make_SUB(loc);    }
"*"     { return yy::parser::make_MUL(loc);    }
"/"     { return yy::parser::make_DIV(loc);    }
"%"     { return yy::parser::make_MODULO(loc); }

    /*关系运算符*/
"<"     { return yy::parser::make_LT(loc); }
">"     { return yy::parser::make_GT(loc); }
"!="    { return yy::parser::make_NE(loc); }
"=="    { return yy::parser::make_EQ(loc); }
"<="    { return yy::parser::make_LE(loc); }
">="    { return yy::parser::make_GE(loc); }

    /*逻辑运算符*/
"&&"    { return yy::parser::make_AND(loc);    }
"||"    { return yy::parser::make_OR(loc);     }
"!"     { return yy::parser::make_NOT(loc);    }

    /*关键字*/
"void"      { return yy::parser::make_VOID(loc);     }
"int"       { return yy::parser::make_INT(loc);      }
"float"     { return yy::parser::make_FLOAT(loc);    }
"const"     { return yy::parser::make_CONST(loc);    }
"if"        { return yy::parser::make_IF(loc);       }
"else"      { return yy::parser::make_ELSE(loc);     }
"while"     { return yy::parser::make_WHILE(loc);    } 
"break"     { return yy::parser::make_BREAK(loc);    }
"continue"  { return yy::parser::make_CONTINUE(loc); }
"return"    { return yy::parser::make_RETURN(loc);   }

[a-zA-Z_][a-zA-Z0-9_]* { return yy::parser::make_IDENT(yytext, loc); }

"/*"                    { BEGIN(COMMENT); loc.step(); }
<COMMENT>"*/"           { BEGIN(INITIAL); loc.step(); }
<COMMENT>[^*\n]+|.      { /* ignore multi-line comments */ loc.step(); }
<COMMENT>\n             { loc.lines(yyleng); loc.step(); }
<COMMENT><<EOF>>        { return yy::parser::make_YYEOF(loc); }
"//".*                  { /* ignore single-line comments */ loc.step(); }

[ \t]   { /* ignore whitespace */ loc.step();}
\n      { loc.lines(yyleng); loc.step();}
.       { /* invalid character */ loc.step();}
<<EOF>> { 
            // 如果有错误，返回错误状态
            if (isOverflow) {
                exit(1);
            }
            return yy::parser::make_YYEOF(loc); 
        }
%%
