#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>

#include "codegen.h"
#include "errors.h"
#include "ast.h"

llvm::Value* LLVMCodegen::VisitNumber(NumberExprAST* const ast) {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(ast->Val));
}

llvm::Value* LLVMCodegen::VisitVariable(VariableExprAST* const ast) {
  // Look this variable up in the function.
  llvm::Value *V = NamedValues[ast->Name];
  if (!V)
    LogErrorV("Unknown variable name");
  return V;
}

llvm::Value* LLVMCodegen::VisitBinaryExpr(BinaryExprAST* const ast) {
  auto L = ast->LHS->accept(*this);
  auto R = ast->RHS->accept(*this);

  if (!L || !R)
    return nullptr;

  switch (ast->Op) {
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
  llvm::Function *CalleeF = TheModule->getFunction(ast->Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  // If argument mismatch error.
  if (CalleeF->arg_size() != ast->Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<llvm::Value *> ArgsV;
  for (unsigned i = 0, e = ast->Args.size(); i != e; ++i) {
    ArgsV.push_back(ast->Args[i]->accept(*this));
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Function* LLVMCodegen::VisitPrototype(PrototypeAST* const ast) {
  // Make the function type:  double(double,double) etc.
  std::vector<llvm::Type *> Doubles(ast->Args.size(), llvm::Type::getDoubleTy(*TheContext));
  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);

  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, ast->Name, TheModule);

  // Set names for all arguments.
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(ast->Args[Idx++]);

  return F;
}

llvm::Function* LLVMCodegen::VisitFunction(FunctionAST* const ast) {
  // TODO: https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/LangImpl03.html#function-code-generation

  // First, check for an existing function from a previous 'extern' declaration.
  llvm::Function *TheFunction = TheModule->getFunction(ast->Proto->getName());
  if (!TheFunction)
    TheFunction = ast->Proto->accept(*this);
  if (!TheFunction)
    return nullptr;

  // Create a new basic block to start insertion into.
  auto BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  // Record the function arguments in the NamedValues map.
  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;
  
  if (llvm::Value *RetVal = ast->Body->accept(*this)) {
    // Finish off the function.
    Builder->CreateRet(RetVal);

    // Validate the generated code, checking for consistency.
    llvm::verifyFunction(*TheFunction);

    return TheFunction;
  }
  TheFunction->eraseFromParent();
  return nullptr;
}