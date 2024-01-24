#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <memory>

#include "parser.h"
#include "codegen.h"
#include "../include/KaleidoscopeJIT.h"

class Interpreter {
  std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
  std::unique_ptr<Parser> TheParser;
  std::unique_ptr<Codegen> TheCodegen;

public:
  Interpreter(std::unique_ptr<Parser> parser, std::unique_ptr<Codegen> codegen) : TheParser(std::move(parser)) {
    llvm::ExitOnError ExitOnErr;
    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
    TheCodegen = std::move(codegen);
    TheCodegen->NewModule(TheJIT->getDataLayout());
  };

  // Starts an interpreter
  void MainLoop();
  Parser *GetParser() { return TheParser.get(); }
  Codegen *GetCodegen() { return TheCodegen.get(); }

private:
  void HandleDefinition();
  void HandleExtern();
  void HandleTopLevelExpression();
};

#endif