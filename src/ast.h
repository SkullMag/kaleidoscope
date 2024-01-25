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
  double Val;

public:
  NumberExprAST(double Val): Val(Val) {}
  llvm::Value* accept(Codegen& visitor);
  double GetVal();
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  llvm::Value* accept(Codegen& visitor);
  std::string &GetName();
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
    : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  llvm::Value* accept(Codegen& visitor);

  char GetOp();
  ExprAST *GetLHS();
  ExprAST *GetRHS();
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
    : Callee(Callee), Args(std::move(Args)) {}
  llvm::Value* accept(Codegen& visitor);
  std::string &GetCallee();
  std::vector<std::unique_ptr<ExprAST>> &GetArgs();
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
    : Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
  llvm::Function* accept(Codegen& visitor);
  std::string &GetName();
  std::vector<std::string> &GetArgs();
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
    : Proto(std::move(Proto)), Body(std::move(Body)) {}
  llvm::Function* accept(Codegen& visitor);

  std::unique_ptr<PrototypeAST> GetProto();
  ExprAST *GetBody();
};

#endif