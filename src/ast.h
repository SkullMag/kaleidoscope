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
  bool _isOperator;
  unsigned Precedence;  // Precedence if a binary op.

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args, bool isOperator, unsigned precedence)
    : Name(Name), Args(std::move(Args)), _isOperator(isOperator), Precedence(precedence) {}

  llvm::Function* accept(Codegen& visitor);
  std::string &GetName();
  std::vector<std::string> &GetArgs();
  
  bool IsUnaryOp() const { return _isOperator && Args.size() == 1; }
  bool IsBinaryOp() const { return _isOperator && Args.size() == 2; }
  bool IsOperator() const { return _isOperator; }

  char GetOperatorName() const {
    assert(IsUnaryOp() || IsBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned GetBinaryPrecedence() const { return Precedence; }
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

/// IfExprAST - Expression class for if/then/else.
class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
    : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  llvm::Value* accept(Codegen& visitor);

  std::unique_ptr<ExprAST> GetCond() { return std::move(Cond); }
  std::unique_ptr<ExprAST> GetThen() { return std::move(Then); }
  std::unique_ptr<ExprAST> GetElse() { return std::move(Else); }

};

/// ForExprAST - Expression class for for/in.
class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
    : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
      Step(std::move(Step)), Body(std::move(Body)) {}
  llvm::Value* accept(Codegen& visitor);

  std::string& GetVarName() { return VarName; }
  std::unique_ptr<ExprAST> GetStart() { return std::move(Start); }
  std::unique_ptr<ExprAST> GetEnd() { return std::move(End); }
  std::unique_ptr<ExprAST> GetStep() { return std::move(Step); }
  std::unique_ptr<ExprAST> GetBody() { return std::move(Body); }

};

/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
    : Opcode(Opcode), Operand(std::move(Operand)) {}

  llvm::Value *accept(Codegen& visitor) override;
  char GetOpcode() { return Opcode; }
  std::unique_ptr<ExprAST> GetOperand() { return std::move(Operand); }
};

#endif