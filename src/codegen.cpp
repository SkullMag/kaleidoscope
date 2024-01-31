#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/Scalar.h>

#include "codegen.h"
#include "errors.h"
#include "ast.h"

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
llvm::AllocaInst *LLVMCodegen::CreateEntryBlockAlloca(llvm::Function *TheFunction, const std::string &VarName) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr, VarName);
}

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
  TheFPM->addPass(llvm::PromotePass());
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
  llvm::AllocaInst *A = NamedValues[ast->GetName()];
  if (!A)
    return LogErrorV("Unknown variable name");
  // Load the value
  return Builder->CreateLoad(A->getAllocatedType(), A, ast->GetName().c_str());
}

llvm::Value* LLVMCodegen::VisitBinaryExpr(BinaryExprAST* const ast) {
  char Op = ast->GetOp();
  // Special case '=' because we don't want to emit the LHS as an expression.
  if (Op == '=') {
    // This assume we're building without RTTI because LLVM builds that way by
    // default. If you build LLVM with RTTI this can be changed to a
    // dynamic_cast for automatic error checking.
    VariableExprAST *LHSE = static_cast<VariableExprAST*>(ast->GetLHS());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");
    
    // Codegen the RHS.
    llvm::Value *Val = ast->GetRHS()->accept(*this);
    if (!Val)
      return nullptr;

    // Look up the name.
    llvm::Value *Variable = NamedValues[LHSE->GetName()];
    if (!Variable)
      return LogErrorV("Unknown variable name");

    Builder->CreateStore(Val, Variable);
    return Val;
  }
  auto L = ast->GetLHS()->accept(*this);
  auto R = ast->GetRHS()->accept(*this);

  if (!L || !R)
    return nullptr;

  switch (Op) {
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
    break;
  }

  // If it wasn't a builtin binary operator, it must be a user defined one. Emit
  // a call to it.
  llvm::Function *F = getFunction(std::string("binary") + ast->GetOp());
  assert(F && "binary operator not found!");

  llvm::Value *Ops[2] = { L, R };
  return Builder->CreateCall(F, Ops, "binop");
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
  llvm::Function *TheFunction = getFunction(P.GetName());
  if (!TheFunction)
    return nullptr;

  // Create a new basic block to start insertion into.
  auto BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    // Create an alloca for this variable.
    std::string ArgName = std::string(Arg.getName());
    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, ArgName);
    // Store the initial value into the alloca.
    Builder->CreateStore(&Arg, Alloca);
    // Add arguments to variable symbol table.
    NamedValues[ArgName] = Alloca;
  }
  
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

// Output for-loop as:
//   var = alloca double
//   ...
//   start = startexpr
//   store start -> var
//   goto loop
// loop:
//   ...
//   bodyexpr
//   ...
// loopend:
//   step = stepexpr
//   endcond = endexpr
//
//   curvar = load var
//   nextvar = curvar + step
//   store nextvar -> var
//   br endcond, loop, endloop
// outloop:
llvm::Value* LLVMCodegen::VisitFor(ForExprAST* const ast) {
  llvm::Function* TheFunction = Builder->GetInsertBlock()->getParent();
  std::string VarName = ast->GetVarName();

  // Create an alloca for the variable in the entry block.
  llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

  // Emit the start code first, without 'variable' in scope.
  llvm::Value *StartVal = ast->GetStart()->accept(*this);
  if (!StartVal)
    return nullptr;
  
  // Store the value into the alloca.
  Builder->CreateStore(StartVal, Alloca);

  // Make the new basic block for the loop header, inserting after current block.
  llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  // Insert an explicit fall through from the current block to the LoopBB.
  Builder->CreateBr(LoopBB);

  // Start insertion in LoopBB.
  Builder->SetInsertPoint(LoopBB);

  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  llvm::AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

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

  // Compute the end condition.
  llvm::Value *EndCond = ast->GetEnd()->accept(*this);
  if (!EndCond)
    return nullptr;
  
  // Reload, increment, and restore the alloca. This handles the case where the body of the loop mutates the variable.
  llvm::Value *CurVar = Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
  llvm::Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  Builder->CreateStore(NextVar, Alloca);

  // Convert condition to a bool by comparing non-equal to 0.0.
  EndCond = Builder->CreateFCmpONE(EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

  // Create the "after loop" block and insert it.
  llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  // Insert the conditional branch into the end of LoopEndBB.
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

  // Any new code will be inserted in AfterBB.
  Builder->SetInsertPoint(AfterBB);

  // Restore the unshadowed variable.
  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  // for expr always returns 0.0.
  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
}

llvm::Value* LLVMCodegen::VisitUnary(UnaryExprAST* const ast) {
  llvm::Value *OperandV = ast->GetOperand()->accept(*this);
  if (!OperandV)
    return nullptr;

  llvm::Function *F = getFunction(std::string("unary") + ast->GetOpcode());
  if (!F)
    return LogErrorV("Unknown unary operator");

  return Builder->CreateCall(F, OperandV, "unop");
}

llvm::Value* LLVMCodegen::VisitVar(VarExprAST* const ast) {
  std::vector<llvm::AllocaInst *> OldBindings;

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  auto VarNames = ast->GetVarNames();
  for (unsigned i = 0, e = VarNames->size(); i != e; ++i) {
    const std::string &VarName = (*VarNames)[i].first;
    ExprAST *Init = (*VarNames)[i].second.get();
    // Emit the initializer before adding the variable to scope, this prevents
    // the initializer from referencing the variable itself, and permits stuff
    // like this:
    //  var a = 1 in
    //    var a = a in ...   # refers to outer 'a'.
    llvm::Value *InitVal;
    if (Init) {
      InitVal = Init->accept(*this);
      if (!InitVal)
        return nullptr;
    } else { // If not specified, use 0.0.
      InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
    }

    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    // Remember the old variable binding so that we can restore the binding when
    // we unrecurse.
    OldBindings.push_back(NamedValues[VarName]);

    // Remember this binding.
    NamedValues[VarName] = Alloca;
  }
  // Codegen the body, now that all vars are in scope.
  llvm::Value *BodyVal = ast->GetBody()->accept(*this);
  if (!BodyVal)
    return nullptr;
  
  // Pop all our variables from scope.
  for (size_t i = 0, e = VarNames->size(); i != e; ++i)
    NamedValues[(*VarNames)[i].first] = OldBindings[i];

  // Return the body computation.
  return BodyVal;
}