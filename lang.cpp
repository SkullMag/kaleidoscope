#include <llvm/Support/TargetSelect.h>

#include "src/parser.h"
#include "src/interpreter.h"
#include "src/codegen.h"

static llvm::ExitOnError ExitOnErr;

int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto interpreter = std::make_unique<Interpreter>(
    std::move(std::make_unique<Parser>(std::move(std::make_unique<Lexer>()))),  // Parser
    std::move(std::make_unique<LLVMCodegen>()) // Codegen
  );
  auto parser = interpreter->GetParser();

  // Install standard binary operators.
  // 1 is lowest precedence.
  parser->AddBinop('<', 10);
  parser->AddBinop('+', 20);
  parser->AddBinop('-', 30);
  parser->AddBinop('*', 40);

  // Prime the first token.
  fprintf(stderr, "ready> ");
  parser->getNextToken();

  // Run the main "interpreter loop" now.
  interpreter->MainLoop();

  return 0;
}