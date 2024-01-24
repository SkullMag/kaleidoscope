#include <string>
#include <vector>
#include <map>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include "src/errors.h"
#include "src/parser.h"
#include "src/interpreter.h"
#include "src/codegen.h"

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<Lexer> TheLexer;
static std::unique_ptr<Parser> TheParser;
static std::unique_ptr<Interpreter> TheInterpreter;
static std::unique_ptr<LLVMCodegen> TheCodegen;

static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
static std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
static std::unique_ptr<llvm::StandardInstrumentations> TheSI;

static void InitializeModule() {
  // Open a new context and module.
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("my cool jit", *TheContext);

  // Create a new builder for the module.
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

  // Create pass and analysis managers
  TheFPM = std::make_unique<llvm::FunctionPassManager>();
  TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
  TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
  TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
  TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();
  ThePIC = std::make_unique<llvm::PassInstrumentationCallbacks>();
  TheSI = std::make_unique<llvm::StandardInstrumentations>(*TheContext, true);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  // Add transform passes.
  TheFPM->addPass(llvm::InstCombinePass());
  TheFPM->addPass(llvm::ReassociatePass());
  TheFPM->addPass(llvm::GVNPass());
  TheFPM->addPass(llvm::SimplifyCFGPass());

  // Register analysis passes used in these transform passes.
  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);

  // Create interpreter
  TheLexer = std::make_unique<Lexer>();
  TheParser = std::make_unique<Parser>(std::move(TheLexer));
  TheCodegen = std::make_unique<LLVMCodegen>(std::move(TheContext), std::move(Builder), std::move(TheModule), std::move(TheFPM), std::move(TheFAM));
  TheInterpreter = std::make_unique<Interpreter>(std::move(TheParser), std::move(TheCodegen));
}

int main() {
  InitializeModule();

  // Install standard binary operators.
  // 1 is lowest precedence.
  TheInterpreter->TheParser->AddBinop('<', 10);
  TheInterpreter->TheParser->AddBinop('+', 20);
  TheInterpreter->TheParser->AddBinop('-', 30);
  TheInterpreter->TheParser->AddBinop('*', 40);

  // Prime the first token.
  fprintf(stderr, "ready> ");
  TheInterpreter->TheParser->getNextToken();

  // Run the main "interpreter loop" now.
  TheInterpreter->MainLoop();

  // Print out all of the generated code.
  TheInterpreter->TheCodegen->getModule()->print(llvm::errs(), nullptr);

  return 0;
}