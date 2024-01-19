#include <llvm/IR/Verifier.h>

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
    if (auto *FnIR = FnAST->codegen()) {
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
    if (auto *ProtoIR = ProtoAST->codegen()) {
      fprintf(stderr, "Parsed an extern\n");
      ProtoIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}

void Interpreter::HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = TheParser->ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Parsed a top-level expr\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      FnIR->eraseFromParent();
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}