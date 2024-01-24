#ifndef CODEGEN_H
#define CODEGEN_H

#include <map>
#include <string>
#include <iostream>

#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>

#include "ast.h"


class Codegen {
public:
  llvm::LLVMContext* TheContext;
  llvm::IRBuilder<>* Builder;
  llvm::Module* TheModule;
  llvm::FunctionPassManager* TheFPM;
  llvm::FunctionAnalysisManager* TheFAM;
  std::map<std::string, llvm::Value *> NamedValues;

  virtual llvm::Value* VisitNumber(NumberExprAST* const ast) = 0;
  virtual llvm::Value* VisitVariable(VariableExprAST* const ast) = 0;
  virtual llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast) = 0;
  virtual llvm::Value* VisitCall(CallExprAST* const ast) = 0;
  virtual llvm::Function* VisitPrototype(PrototypeAST* const ast) = 0;
  virtual llvm::Function* VisitFunction(FunctionAST* const ast) = 0;
  virtual std::unique_ptr<llvm::Module> &getModule() = 0;
  virtual ~Codegen() = default;
};

class LLVMCodegen: public Codegen {
public:
  std::unique_ptr<llvm::LLVMContext> TheContext;
  std::unique_ptr<llvm::IRBuilder<>> Builder;
  std::unique_ptr<llvm::Module> TheModule;
  std::unique_ptr<llvm::FunctionPassManager> TheFPM;
  std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
  std::map<std::string, llvm::Value *> NamedValues;

  LLVMCodegen(std::unique_ptr<llvm::LLVMContext> ctx, std::unique_ptr<llvm::IRBuilder<>> builder, std::unique_ptr<llvm::Module> module, std::unique_ptr<llvm::FunctionPassManager> fpm, std::unique_ptr<llvm::FunctionAnalysisManager> fam) 
    : TheContext(std::move(ctx)), Builder(std::move(builder)), TheModule(std::move(module)), TheFPM(std::move(fpm)), TheFAM(std::move(fam)) {
      std::cout << "Module in codegen " << TheModule << std::endl;
    };

  llvm::Value* VisitNumber(NumberExprAST* const ast);
  llvm::Value* VisitVariable(VariableExprAST* const ast);
  llvm::Value* VisitBinaryExpr(BinaryExprAST* const ast);
  llvm::Value* VisitCall(CallExprAST* const ast);
  llvm::Function* VisitPrototype(PrototypeAST* const ast);
  llvm::Function* VisitFunction(FunctionAST* const ast);

  std::unique_ptr<llvm::Module> &getModule() { return TheModule; }
};

#endif