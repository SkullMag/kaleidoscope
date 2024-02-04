#include <llvm/Support/TargetSelect.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/TargetParser/Host.h>

#include "src/parser.h"
#include "src/interpreter.h"
#include "src/codegen.h"

static llvm::ExitOnError ExitOnErr;

extern "C" double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}
extern "C" double printd(double X) {
  printf("%f\n", X);
  return 0;
}

int main() {
   // Initialize the target registry etc.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  auto TargetTriple = llvm::sys::getDefaultTargetTriple();

  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

  // Print an error and exit if we couldn't find the requested target.
  // This generally occurs if we've forgotten to initialise the
  // TargetRegistry or we have a bogus target triple.
  if (!Target) {
    llvm::errs() << Error;
    return 1;
  }

  auto CPU = "generic";
  auto Features = "";

  llvm::TargetOptions opt;
  auto TheTargetMachine = Target->createTargetMachine(TargetTriple, CPU, Features, opt, llvm::Reloc::PIC_);

  auto interpreter = std::make_unique<Interpreter>(
    std::move(std::make_unique<Parser>(std::move(std::make_unique<Lexer>()))),  // Parser
    std::move(std::make_unique<LLVMCodegen>()), // Codegen
    TheTargetMachine->createDataLayout(),
    TargetTriple
  );
  auto parser = interpreter->GetParser();

  // Install standard binary operators.
  // 1 is lowest precedence.
  parser->AddBinop('=', 2);
  parser->AddBinop('<', 10);
  parser->AddBinop('+', 20);
  parser->AddBinop('-', 30);
  parser->AddBinop('*', 40);

  // Prime the first token.
  fprintf(stderr, "ready> ");
  parser->getNextToken();

  // Run the main "interpreter loop" now.
  interpreter->MainLoop();

  auto TheModule = std::move(interpreter->GetCodegen()->getModule());
  auto Filename = "output.o";
  std::error_code EC;
  llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message();
    return 1;
  }

  llvm::legacy::PassManager pass;
  auto FileType = llvm::CodeGenFileType::CGFT_ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    llvm::errs() << "TheTargetMachine can't emit a file of this type";
    return 1;
  }

  pass.run(*TheModule);
  dest.flush();

  llvm::outs() << "Wrote " << Filename << "\n";
  return 0;
}
