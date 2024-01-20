#ifndef AST_H
#define AST_H

#include <string>
#include <vector>

#include <llvm/IR/Value.h>

class Codegen;

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value* accept(Codegen& visitor) = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST: public ExprAST {
public:
  double Val;

  NumberExprAST(double Val): Val(Val) {}
  llvm::Value* accept(Codegen& visitor);
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
public:
  std::string Name;

  VariableExprAST(const std::string &Name) : Name(Name) {}
  llvm::Value* accept(Codegen& visitor);
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
public:
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  llvm::Value* accept(Codegen& visitor);
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
public:
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
  llvm::Value* accept(Codegen& visitor);
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
public:
  std::string Name;
  std::vector<std::string> Args;

  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
  llvm::Function* accept(Codegen& visitor);
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
public:
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
  llvm::Function* accept(Codegen& visitor);
};

#endif