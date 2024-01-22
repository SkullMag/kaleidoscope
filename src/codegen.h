#ifndef CODEGEN_H
#define CODEGEN_H

#include <map>
#include <string>

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

#include "ast.h"


class Codegen {
public:
  virtual llvm::Value* VisitNumber(NumberExprAST* const ast) = 0;
  virtual llvm::Value* VisitVariable(VariableExprAST* const ast) = 0;
  virtual llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast) = 0;
  virtual llvm::Value* VisitCall(CallExprAST* const ast) = 0;
  virtual llvm::Function* VisitPrototype(PrototypeAST* const ast) = 0;
  virtual llvm::Function* VisitFunction(FunctionAST* const ast) = 0;
};

class LLVMCodegen: public Codegen {
  llvm::LLVMContext* TheContext;
  llvm::IRBuilder<>* Builder;
  llvm::Module* TheModule;
  llvm::FunctionPassManager* TheFPM;
  llvm::FunctionAnalysisManager* TheFAM;
  std::map<std::string, llvm::Value *> NamedValues;

public:
  LLVMCodegen(llvm::LLVMContext* ctx, llvm::IRBuilder<>* builder, llvm::Module* module, llvm::FunctionPassManager* fpm, llvm::FunctionAnalysisManager* fam)
    : TheContext(ctx), Builder(builder), TheModule(module), TheFPM(fpm), TheFAM(fam) {};

  llvm::Value* VisitNumber(NumberExprAST* const ast);
  llvm::Value* VisitVariable(VariableExprAST* const ast);
  llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast);
  llvm::Value* VisitCall(CallExprAST* const ast);
  llvm::Function* VisitPrototype(PrototypeAST* const ast);
  llvm::Function* VisitFunction(FunctionAST* const ast);
};

#endif