#include <iostream>

#include <llvm/IR/Verifier.h>
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"

#include "interpreter.h"
#include "toks.h"

/// top ::= definition | external | expression | ';'
void Interpreter::MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (TheParser->CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      TheParser->getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

void Interpreter::HandleDefinition() {
  if (auto FnAST = TheParser->ParseDefinition()) {
    if (auto *FnIR = FnAST->accept(*TheCodegen)) {
      fprintf(stderr, "Parsed a function definition.\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}

void Interpreter::HandleExtern() {
  if (auto ProtoAST = TheParser->ParseExtern()) {
    if (auto *ProtoIR = ProtoAST->accept(*TheCodegen)) {
      fprintf(stderr, "Parsed an extern\n");
      ProtoIR->print(llvm::errs());
      fprintf(stderr, "\n");
      TheCodegen->addFunctionProto(ProtoAST->GetName(), std::move(ProtoAST));
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}

void Interpreter::HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = TheParser->ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->accept(*TheCodegen)) {
      fprintf(stderr, "Parsed a top-level expression.\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}