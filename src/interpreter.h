#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <memory>

#include "parser.h"
#include "codegen.h"

class Interpreter {
  Parser* TheParser;
  Codegen* TheCodegen;

public:
  Interpreter(Parser* parser, Codegen* codegen) : TheParser(parser), TheCodegen(codegen) {};

  // Starts an interpreter
  void MainLoop();

private:
  void HandleDefinition();
  void HandleExtern();
  void HandleTopLevelExpression();
};

#endif