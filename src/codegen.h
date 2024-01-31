#ifndef CODEGEN_H
#define CODEGEN_H

#include <map>
#include <string>

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>

#include "ast.h"

class Codegen {
public:
  virtual llvm::Value* VisitNumber(NumberExprAST* const ast) = 0;
  virtual llvm::Value* VisitVariable(VariableExprAST* const ast) = 0;
  virtual llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast) = 0;
  virtual llvm::Value* VisitCall(CallExprAST* const ast) = 0;
  virtual llvm::Function* VisitPrototype(PrototypeAST* const ast) = 0;
  virtual llvm::Function* VisitFunction(FunctionAST* const ast) = 0;
  virtual llvm::Value* VisitIf(IfExprAST* const ast) = 0;
  virtual llvm::Value* VisitFor(ForExprAST* const ast) = 0;
  virtual llvm::Value* VisitUnary(UnaryExprAST* const ast) = 0;

  virtual void NewModule(const llvm::DataLayout &layout) = 0;
  virtual std::unique_ptr<llvm::Module> &getModule() = 0;
  virtual std::unique_ptr<llvm::LLVMContext> &getContext() = 0;
  virtual llvm::Function *getFunction(std::string name) = 0;
  virtual void addFunctionProto(std::string name, std::unique_ptr<PrototypeAST> proto) = 0;
  virtual ~Codegen() = default;
};

class LLVMCodegen: public Codegen {
  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::FunctionPassManager> TheFPM;
  std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
  std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
  std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
  std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
  std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
  std::unique_ptr<llvm::StandardInstrumentations> TheSI;
  std::map<std::string, llvm::AllocaInst *> NamedValues;
  std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

public:
  llvm::Value* VisitNumber(NumberExprAST* const ast);
  llvm::Value* VisitVariable(VariableExprAST* const ast);
  llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast);
  llvm::Value* VisitCall(CallExprAST* const ast);
  llvm::Function* VisitPrototype(PrototypeAST* const ast);
  llvm::Function* VisitFunction(FunctionAST* const ast);
  llvm::Value* VisitIf(IfExprAST* const ast);
  llvm::Value* VisitFor(ForExprAST* const ast);
  llvm::Value* VisitUnary(UnaryExprAST* const ast);

  void NewModule(const llvm::DataLayout &layout);
  std::unique_ptr<llvm::Module> &getModule() { return TheModule; }
  std::unique_ptr<llvm::LLVMContext> &getContext() { return TheContext; }
  llvm::Function *getFunction(std::string name);
  void addFunctionProto(std::string name, std::unique_ptr<PrototypeAST> proto);

private:
  llvm::AllocaInst* CreateEntryBlockAlloca(llvm::Function *TheFunction, const std::string &VarName);
};

#endif