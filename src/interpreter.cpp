#include <iostream>

#include <llvm/IR/Verifier.h>
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"

#include "interpreter.h"
#include "toks.h"

static llvm::ExitOnError ExitOnErr;
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
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}

void Interpreter::HandleTopLevelExpression() {
  llvm::ExitOnError ExitOnErr;

  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = TheParser->ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->accept(*TheCodegen)) {
      // Create a ResourceTracker to track JIT'd memory allocated to our
      // anonymous expression -- that way we can free it after executing.
      auto RT = TheJIT->getMainJITDylib().createResourceTracker();

      auto TSM = llvm::orc::ThreadSafeModule(std::move(TheCodegen->getModule()), std::move(TheCodegen->getContext()));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
      TheCodegen->NewModule(TheJIT->getDataLayout());

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));
      // assert(ExprSymbol && "Function not found");

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = ExprSymbol.getAddress().toPtr<double (*)()>();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      ExitOnErr(RT->remove());
    }
  } else {
    // Skip token for error recovery.
    TheParser->getNextToken();
  }
}