CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Iinclude -I. -Ithird_part/compiler_ir/include

BUILD_DIR := build

INPUT ?= tests/ok_001_minimal_return.sy
TOKENS ?= $(BUILD_DIR)/token.tsv
AST ?= $(BUILD_DIR)/ast.txt
REDUCE ?= $(BUILD_DIR)/reduce.txt
IR ?= $(BUILD_DIR)/output.ll

LEXER_BIN := $(BUILD_DIR)/lexer_test
PARSER_BIN := $(BUILD_DIR)/parser_test
IR_BIN := $(BUILD_DIR)/ir_test
EDITOR_BIN := $(BUILD_DIR)/cminus_editor
COMPILER_BIN := $(BUILD_DIR)/c--compiler

LEXER_SRCS := \
	tests/drivers/lexer_main.cpp \
	src/lexer/lexer.cpp \
	src/lexer/automata.cpp

PARSER_SRCS := \
	tests/drivers/parser_main.cpp \
	src/parser/parser.cpp

IR_LIB_SRCS := \
	third_part/compiler_ir/src/BasicBlock.cpp \
	third_part/compiler_ir/src/Constant.cpp \
	third_part/compiler_ir/src/Function.cpp \
	third_part/compiler_ir/src/GlobalVariable.cpp \
	third_part/compiler_ir/src/IRprinter.cpp \
	third_part/compiler_ir/src/Instruction.cpp \
	third_part/compiler_ir/src/Module.cpp \
	third_part/compiler_ir/src/Type.cpp \
	third_part/compiler_ir/src/User.cpp \
	third_part/compiler_ir/src/Value.cpp

IR_SRCS := \
	tests/drivers/ir_main.cpp \
	src/ir/IRGenerator.cpp \
	$(IR_LIB_SRCS)

EDITOR_SRCS := \
	src/tools/cminus_editor.cpp \
	src/lexer/lexer.cpp \
	src/lexer/automata.cpp \
	src/parser/parser.cpp

COMMON_SRCS := \
	src/common/Config.cpp \
	src/common/FileUtil.cpp \
	src/common/Token.cpp \
	src/parser/AST.cpp

COMPILER_SRCS := \
	src/main.cpp \
	src/tools/cminus_editor.cpp \
	src/lexer/lexer.cpp \
	src/lexer/automata.cpp \
	src/parser/parser.cpp \
	src/semantic/SemanticAnalyzer.cpp \
	src/ir/IRGenerator.cpp \
	$(COMMON_SRCS) \
	$(IR_LIB_SRCS)

FILE ?= $(BUILD_DIR)/editor_buffer.sy
OUTPUT ?=

.PHONY: all lexer_test parser_test ir_test editor c--compiler run-lexer run-parser run-ir run-editor run-compiler clean-temp clean

all: lexer_test parser_test ir_test editor c--compiler

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

lexer_test: $(LEXER_BIN)

parser_test: $(PARSER_BIN)

ir_test: $(IR_BIN)

editor: $(EDITOR_BIN)

c--compiler: $(COMPILER_BIN)

$(LEXER_BIN): $(LEXER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(LEXER_SRCS) -o $(LEXER_BIN)

$(PARSER_BIN): $(PARSER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(PARSER_SRCS) -o $(PARSER_BIN)

$(IR_BIN): $(IR_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(IR_SRCS) -o $(IR_BIN)

$(EDITOR_BIN): $(EDITOR_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(EDITOR_SRCS) -lncurses -o $(EDITOR_BIN)

$(COMPILER_BIN): $(COMPILER_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -DCMINUS_EDITOR_NO_MAIN $(COMPILER_SRCS) -lncurses -o $(COMPILER_BIN)

run-lexer: $(LEXER_BIN)
	$(LEXER_BIN) $(INPUT) $(TOKENS)
	@echo "token output: $(TOKENS)"

run-parser: $(PARSER_BIN)
	$(PARSER_BIN) $(TOKENS) $(AST) $(REDUCE)
	@echo "ast output: $(AST)"
	@echo "reduce output: $(REDUCE)"

run-ir: $(IR_BIN)
	$(IR_BIN) $(AST) $(IR)
	@echo "ir output: $(IR)"

run-editor: $(EDITOR_BIN)
	$(EDITOR_BIN) $(FILE)

run-compiler: $(COMPILER_BIN)
	$(COMPILER_BIN) $(INPUT) $(if $(OUTPUT),-o $(OUTPUT),)

clean-temp:
	rm -rf $(BUILD_DIR)

clean: clean-temp
