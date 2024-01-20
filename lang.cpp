#include <string>
#include <vector>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "src/errors.h"
#include "src/parser.h"
#include "src/interpreter.h"
#include "src/codegen.h"

// ========================================
// LLVM variables
// ========================================
static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::unique_ptr<llvm::Module> TheModule;

static std::unique_ptr<Parser> TheParser;
static std::unique_ptr<Interpreter> TheInterpreter;
static std::unique_ptr<LLVMCodegen> TheCodegen;

static void InitializeModule() {
  // Open a new context and module.
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("my cool jit", *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

  // Create new parser
  TheParser = std::make_unique<Parser>();

  // Create codegen
  TheCodegen = std::make_unique<LLVMCodegen>(TheContext.get(), Builder.get(), TheModule.get());

  // Create the interpreter
  TheInterpreter = std::make_unique<Interpreter>(TheParser.get(), TheCodegen.get());
}

int main() {
  InitializeModule();

  // Install standard binary operators.
  // 1 is lowest precedence.
  TheParser->AddBinop('<', 10);
  TheParser->AddBinop('+', 20);
  TheParser->AddBinop('-', 30);
  TheParser->AddBinop('*', 40);

  // Prime the first token.
  fprintf(stderr, "ready> ");
  TheParser->getNextToken();

  // Run the main "interpreter loop" now.
  TheInterpreter->MainLoop();

  // Print out all of the generated code.
  TheModule->print(llvm::errs(), nullptr);

  return 0;
}