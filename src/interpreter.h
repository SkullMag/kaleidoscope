#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <memory>

#include "parser.h"
#include "codegen.h"

class Interpreter {
  std::unique_ptr<Parser> TheParser;
  std::unique_ptr<Codegen> TheCodegen;

public:
  Interpreter(std::unique_ptr<Parser> parser, std::unique_ptr<Codegen> codegen, llvm::DataLayout layout, llvm::StringRef triple) : TheParser(std::move(parser)) {
    TheCodegen = std::move(codegen);
    TheCodegen->NewModule(layout, triple);
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