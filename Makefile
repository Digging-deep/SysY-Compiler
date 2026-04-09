# ==============================================================================
# SysY 编译器 Makefile
# ==============================================================================

# 编译器配置
CXX = clang++
TARGET = syc  # SysY Compiler

# 目录配置
SRC_DIR   = src
INCLUDE_DIR = include
TEST_DIR  = test
LIB_DIR   = lib

# LLVM 配置
LLVM_CONFIG = llvm-config
LLVM_CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags --libs core)

# 编译选项
CXXFLAGS = -I$(INCLUDE_DIR) $(LLVM_CXXFLAGS) -g -fexceptions -frtti
LDFLAGS = $(LLVM_LDFLAGS)

# 源文件
SRC_FILES = \
    $(SRC_DIR)/lexer.cpp \
    $(SRC_DIR)/parser.tab.cpp \
    $(SRC_DIR)/Types.cpp \
    $(SRC_DIR)/ASTPrintVisitor.cpp \
    $(SRC_DIR)/SemaCheckVisitor.cpp \
    $(SRC_DIR)/SCSymTable.cpp \
    $(SRC_DIR)/IRGenVisitor.cpp \
    $(SRC_DIR)/IRGSymTable.cpp \
    $(SRC_DIR)/DebugInfoGenerator.cpp \
    $(SRC_DIR)/Driver.cpp

# 生成的文件
GENERATED_FILES = \
    $(SRC_DIR)/lexer.cpp \
    $(INCLUDE_DIR)/lexer.hpp \
    $(SRC_DIR)/parser.tab.cpp \
    $(INCLUDE_DIR)/parser.tab.hpp \
    $(INCLUDE_DIR)/location.hpp \
    $(SRC_DIR)/parser.output \
	syc

# 默认目标
.DEFAULT_GOAL := all

# 伪目标
.PHONY: all clean run test help

# ==============================================================================
# 构建规则
# ==============================================================================

# 主目标
all: $(TARGET)

# 生成可执行文件
$(TARGET): $(SRC_FILES)
	@echo "Linking $@..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build completed successfully!"

# Bison 生成语法分析器
$(SRC_DIR)/parser.tab.cpp $(INCLUDE_DIR)/parser.tab.hpp: $(SRC_DIR)/parser.ypp $(wildcard $(INCLUDE_DIR)/*.hpp)
	@echo "Generating parser..."
	cd $(SRC_DIR) && bison -v parser.ypp
	@mv $(SRC_DIR)/parser.tab.hpp $(INCLUDE_DIR)/
	@mv $(SRC_DIR)/location.hpp $(INCLUDE_DIR)/

# Flex 生成词法分析器
$(SRC_DIR)/lexer.cpp $(INCLUDE_DIR)/lexer.hpp: $(SRC_DIR)/lexer.l $(INCLUDE_DIR)/parser.tab.hpp
	@echo "Generating lexer..."
	cd $(SRC_DIR) && flex lexer.l
	@mv $(SRC_DIR)/lexer.hpp $(INCLUDE_DIR)/

# ==============================================================================
# 辅助目标
# ==============================================================================

# 运行编译器
run: $(TARGET)
	@echo "Running compiler..."
	./$(TARGET) $(TEST_DIR)/test.sy

# 清理生成文件
clean:
	@echo "Cleaning generated files..."
	@rm -rf $(TARGET) $(GENERATED_FILES)
	@echo "Clean completed!"

# 显示帮助信息
help:
	@echo "SysY Compiler Makefile"
	@echo "======================"
	@echo "make         - Build the compiler"
	@echo "make run     - Build and run the compiler"
	@echo "make clean   - Clean generated files"
	@echo "make help    - Show this help message"

