#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "codegen.h"
#include "errors.h"
#include "ast.h"

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

llvm::Value* LLVMCodegen::VisitCall(CallExprAST* const ast) {
  // Look up the name in the global module table.
  llvm::Function *CalleeF = TheModule->getFunction(ast->GetCallee());
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

  // First, check for an existing function from a previous 'extern' declaration.
  llvm::Function *TheFunction = TheModule->getFunction(ast->GetProto()->getName());
  if (!TheFunction)
    TheFunction = ast->GetProto()->accept(*this);
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