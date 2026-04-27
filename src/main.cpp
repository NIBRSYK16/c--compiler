// src/main.cpp

#include <exception>
#include <iostream>
#include <string>

#include "cminus/common/Config.h"
#include "cminus/common/FileUtil.h"
#include "cminus/common/Result.h"
#include "cminus/lexer/Lexer.h"
#include "cminus/parser/Parser.h"
#include "cminus/ir/IRGenerator.h"

using namespace cminus;

namespace {

int failWithMessage(const std::string& message) {
    std::cerr << "[Error] " << message << std::endl;
    return 1;
}

void printStageInfo(const Config& config) {
    std::cout << "[Info] Input file: " << config.inputFile << std::endl;
    std::cout << "[Info] Stage: " << stageToString(config.stage) << std::endl;
    std::cout << "[Info] Output dir: " << config.outputDir << std::endl;
}

void writeErrorAndRunInfo(
    const Config& config,
    const std::string& outputDir,
    const std::string& errorStage,
    const std::string& errorMessage
) {
    writeTextFile(outputDir + "/error.txt", errorMessage);
    writeRunInfo(config, outputDir, false, errorStage);

    std::cerr << "[Error] " << errorStage << " failed." << std::endl;
    std::cerr << errorMessage << std::endl;
}

} // namespace

int main(int argc, char** argv) {
    try {
        Config config = parseArgs(argc, argv);

        if (config.showHelp) {
            printHelp();
            return 0;
        }

        if (config.inputFile.empty()) {
            printHelp();
            return failWithMessage("missing input file.");
        }

        // 1. 创建输出目录
        // 如果用户没有通过 -o 指定输出目录，则自动生成 output/result_YYYYMMDD_HHMMSS
        if (config.outputDir.empty()) {
            config.outputDir = createTimestampOutputDir();
        } else {
            ensureDirectory(config.outputDir);
        }

        printStageInfo(config);

        // 2. 读取输入文件，并保存一份 input.sy 到本次结果目录
        std::string source = readTextFile(config.inputFile);
        copyInputFile(config.inputFile, config.outputDir);

        // 先创建一个空 error.txt，表示目前没有错误
        writeTextFile(config.outputDir + "/error.txt", "");

        // 3. 词法分析阶段
        std::cout << "[Info] Running lexer..." << std::endl;

        Lexer lexer;
        LexResult lexResult = lexer.tokenize(source);

        writeTokens(lexResult.tokens, config.outputDir + "/token.txt");

        if (!lexResult.success) {
            writeErrorAndRunInfo(
                config,
                config.outputDir,
                "lexer",
                lexResult.errorMessage
            );
            return 1;
        }

        std::cout << "[Info] Lexical analysis finished." << std::endl;

        if (config.stage == Stage::Lex) {
            writeRunInfo(config, config.outputDir, true, "");
            std::cout << "[Info] Finished at lexer stage." << std::endl;
            std::cout << "[Info] Results written to: " << config.outputDir << std::endl;
            return 0;
        }

        // 4. 语法分析阶段
        std::cout << "[Info] Running parser..." << std::endl;

        Parser parser;
        ParseResult parseResult = parser.parse(lexResult.tokens);

        writeLines(parseResult.reduceLogs, config.outputDir + "/reduce.txt");
        writeASTFile(parseResult.root.get(), config.outputDir + "/ast.txt");

        if (!parseResult.success) {
            writeErrorAndRunInfo(
                config,
                config.outputDir,
                "parser",
                parseResult.errorMessage
            );
            return 1;
        }

        std::cout << "[Info] Syntax analysis finished." << std::endl;

        if (config.stage == Stage::Parse) {
            writeRunInfo(config, config.outputDir, true, "");
            std::cout << "[Info] Finished at parser stage." << std::endl;
            std::cout << "[Info] Results written to: " << config.outputDir << std::endl;
            return 0;
        }

        // 5. 中间代码生成阶段
        std::cout << "[Info] Running IR generator..." << std::endl;

        IRGenerator irGenerator;
        IRResult irResult = irGenerator.generate(parseResult.root.get());

        writeTextFile(config.outputDir + "/output.ll", irResult.irText);

        if (!irResult.success) {
            writeErrorAndRunInfo(
                config,
                config.outputDir,
                "ir",
                irResult.errorMessage
            );
            return 1;
        }

        std::cout << "[Info] IR generation finished." << std::endl;

        // 6. 完整流程结束
        writeRunInfo(config, config.outputDir, true, "");

        std::cout << "[Info] Compilation pipeline finished successfully." << std::endl;
        std::cout << "[Info] Results written to: " << config.outputDir << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[Fatal Error] unknown exception." << std::endl;
        return 1;
    }
}
