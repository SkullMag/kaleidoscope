#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Error.h>

#include "codegen.h"
#include "errors.h"
#include "ast.h"

void LLVMCodegen::NewModule(const llvm::DataLayout &layout) {
  // Open a new context and module.
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("my cool jit", *TheContext);
  TheModule->setDataLayout(layout);

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
}

void LLVMCodegen::addFunctionProto(std::string name, std::unique_ptr<PrototypeAST> proto) {
  FunctionProtos[name] = std::move(proto);
}

llvm::Value* LLVMCodegen::VisitNumber(NumberExprAST* const ast) {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(ast->GetVal()));
}

llvm::Value* LLVMCodegen::VisitVariable(VariableExprAST* const ast) {
  // Look this variable up in the function.
  llvm::Value *V = NamedValues[ast->GetName()];
  if (!V)
    return LogErrorV("Unknown variable name");
  return V;
}

llvm::Value* LLVMCodegen::VisitBinaryExpr(BinaryExprAST* const ast) {
  auto L = ast->GetLHS()->accept(*this);
  auto R = ast->GetRHS()->accept(*this);

  if (!L || !R)
    return nullptr;

  switch (ast->GetOp()) {
  case '+':
    return Builder->CreateFAdd(L, R, "addtmp");
  case '-':
    return Builder->CreateFSub(L, R, "subtmp");
  case '*':
    return Builder->CreateFMul(L, R, "multmp");
  case '<':
    L = Builder->CreateFCmpULT(L, R, "cmptmp");
    // Convert bool 0/1 to double 0.0 or 1.0
    return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp");
  default:
    return LogErrorV("invalid binary operator");
  }
}

llvm::Function *LLVMCodegen::getFunction(std::string Name) {
  // First, see if the function has already been added to the current module.
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // If not, check whether we can codegen the declaration from some existing
  // prototype.
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->accept(*this);

  // If no existing prototype exists, return null.
  return nullptr;
}

llvm::Value* LLVMCodegen::VisitCall(CallExprAST* const ast) {
  // Look up the name in the global module table.
  llvm::Function *CalleeF = getFunction(ast->GetCallee());
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  // If argument mismatch error.
  if (CalleeF->arg_size() != ast->GetArgs().size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<llvm::Value *> ArgsV;
  for (unsigned i = 0, e = ast->GetArgs().size(); i != e; ++i) {
    ArgsV.push_back(ast->GetArgs()[i]->accept(*this));
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function* LLVMCodegen::VisitPrototype(PrototypeAST* const ast) {
  // Make the function type:  double(double,double) etc.
  std::vector<llvm::Type *> Doubles(ast->GetArgs().size(), llvm::Type::getDoubleTy(*TheContext));
  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);

  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, ast->GetName(), *TheModule);

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(ast->GetArgs()[Idx++]);

  return F;
}

llvm::Function* LLVMCodegen::VisitFunction(FunctionAST* const ast) {
  // TODO: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl03.html#function-code-generation

  // Transfer ownership of the prototype to the FunctionProtos map, but keep a
  // reference to it for use below.
  auto proto = ast->GetProto();
  auto &P = *proto;
  addFunctionProto(proto->GetName(), std::move(proto));
  llvm::Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  // Create a new basic block to start insertion into.
  auto BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;
  
  if (llvm::Value *RetVal = ast->GetBody()->accept(*this)) {
    // Finish off the function.
    Builder->CreateRet(RetVal);

    // Validate the generated code, checking for consistency.
    llvm::verifyFunction(*TheFunction);

    // Optimize the function
    TheFPM->run(*TheFunction, *TheFAM);

    return TheFunction;
  }
  TheFunction->eraseFromParent();
  return nullptr;
}

llvm::Value* LLVMCodegen::VisitIf(IfExprAST* const ast) {
  llvm::Value* CondV = ast->GetCond()->accept(*this);
  if (!CondV)
    return nullptr;

  // Convert condition to a bool by comparing non-equal to 0.0.
  CondV = Builder->CreateFCmpONE(CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit then value.
  Builder->SetInsertPoint(ThenBB);

  llvm::Value *ThenV = ast->GetThen()->accept(*this);
  if (!ThenV)
    return nullptr;
  Builder->CreateBr(MergeBB);

  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  ThenBB = Builder->GetInsertBlock();

  // Emit else block.
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);

  llvm::Value *ElseV = ast->GetElse()->accept(*this);
  if (!ElseV)
    return nullptr;
  Builder->CreateBr(MergeBB);

  // codegen of 'Else' can change the current block, update ElseBB for the PHI.
  ElseBB = Builder->GetInsertBlock();

  // Emit merge block.
  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  llvm::PHINode *PN = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

llvm::Value* LLVMCodegen::VisitFor(ForExprAST* const ast) {
  // Emit the start code first, without 'variable' in scope.
  llvm::Value *StartV = ast->GetStart()->accept(*this);
  if (!StartV)
    return nullptr;

  // Make the new basic block for the loop header, inserting after current block.
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
  llvm::BasicBlock *PreheaderBB = Builder->GetInsertBlock();
  llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  // Insert an explicit fall through from the current block to the LoopBB.
  Builder->CreateBr(LoopBB);

  // Start insertion in LoopBB.
  Builder->SetInsertPoint(LoopBB);

  // Start the PHI node with an entry for Start.
  llvm::PHINode *Variable = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, ast->GetVarName());
  Variable->addIncoming(StartV, PreheaderBB);

  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  llvm::Value *OldV = NamedValues[ast->GetVarName()];
  NamedValues[ast->GetVarName()] = Variable;

  // Emit the body of the loop.  This, like any other expr, can change the
  // current BB.  Note that we ignore the value computed by the body, but don't
  // allow an error.
  if (!ast->GetBody()->accept(*this))
    return nullptr;
  
  // Emit the step value.
  llvm::Value *StepVal = nullptr;
  auto Step = ast->GetStep();
  if (Step) {
    StepVal = Step->accept(*this);
    if (!StepVal)
      return nullptr;
  } else {
    // If not specified, use 1.0.
    StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
  }
  llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

  // Compute the end condition.
  llvm::Value *EndCond = ast->GetEnd()->accept(*this);
  if (!EndCond)
    return nullptr;
  
  // Convert condition to a bool by comparing non-equal to 0.0.
  EndCond = Builder->CreateFCmpONE(EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

  // Create the "after loop" block and insert it.
  llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock();
  llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  // Insert the conditional branch into the end of LoopEndBB.
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

  // Any new code will be inserted in AfterBB.
  Builder->SetInsertPoint(AfterBB);

    // Add a new entry to the PHI node for the backedge.
  Variable->addIncoming(NextVar, LoopEndBB);

  // Restore the unshadowed variable.
  if (OldV)
    NamedValues[ast->GetVarName()] = OldV;
  else
    NamedValues.erase(ast->GetVarName());

  // for expr always returns 0.0.
  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
}