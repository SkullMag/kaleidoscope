#include <llvm/IR/Value.h>

#include "ast.h"
#include "codegen.h"

// NumberExprAST
double NumberExprAST::GetVal() { return Val; }
llvm::Value* NumberExprAST::accept(Codegen& visitor) { return visitor.VisitNumber(this); }

// VariableExprAST
std::string &VariableExprAST::GetName() { return Name; }
llvm::Value* VariableExprAST::accept(Codegen& visitor) { return visitor.VisitVariable(this); }

// BinaryExprAST
char BinaryExprAST::GetOp() { return Op; }
ExprAST *BinaryExprAST::GetLHS() { return LHS.get(); }
ExprAST *BinaryExprAST::GetRHS() { return RHS.get(); }
llvm::Value* BinaryExprAST::accept(Codegen& visitor) { return visitor.VisitBinaryExpr(this); }

// CallExprAST
std::string &CallExprAST::GetCallee() { return Callee; }
std::vector<std::unique_ptr<ExprAST>> &CallExprAST::GetArgs() { return Args; }
llvm::Value* CallExprAST::accept(Codegen& visitor) { return visitor.VisitCall(this); }

// PrototypeAST
std::string &PrototypeAST::GetName() { return Name; };
std::vector<std::string> &PrototypeAST::GetArgs() { return Args; };
llvm::Function* PrototypeAST::accept(Codegen& visitor) { return visitor.VisitPrototype(this); }

// FunctionAST
std::unique_ptr<PrototypeAST> FunctionAST::GetProto() { return std::move(Proto); }
ExprAST *FunctionAST::GetBody() { return Body.get(); }
llvm::Function* FunctionAST::accept(Codegen& visitor) { return visitor.VisitFunction(this); }

// IfExprAST
llvm::Value *IfExprAST::accept(Codegen& visitor) { return visitor.VisitIf(this); }