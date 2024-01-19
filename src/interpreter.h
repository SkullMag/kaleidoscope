#ifndef INTERPRETER_H
#define INTERPRETER_H

#include <memory>

#include "parser.h"

class Interpreter {
  Parser* TheParser;

public:
  Interpreter(Parser* parser) : TheParser(parser) {};

  // Starts an interpreter
  void MainLoop();

private:
  void HandleDefinition();
  void HandleExtern();
  void HandleTopLevelExpression();
};

#endif