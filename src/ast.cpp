#include <llvm/IR/Value.h>

#include "ast.h"
#include "codegen.h"

llvm::Value* NumberExprAST::accept(Codegen& visitor) {
  return visitor.VisitNumber(this);
}

llvm::Value* VariableExprAST::accept(Codegen& visitor) {
  return visitor.VisitVariable(this);
}

llvm::Value* BinaryExprAST::accept(Codegen& visitor) {
  return visitor.VisitBinaryExpr(this);
}

llvm::Value* CallExprAST::accept(Codegen& visitor) {
  return visitor.VisitCall(this);
}

llvm::Function* PrototypeAST::accept(Codegen& visitor) {
  return visitor.VisitPrototype(this);
}

llvm::Function* FunctionAST::accept(Codegen& visitor) {
  return visitor.VisitFunction(this);
}