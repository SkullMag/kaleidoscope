#include <memory>
#include <map>
#include <math.h>

#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "toks.h"
#include "errors.h"

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
int Parser::getNextToken() {
  return CurTok = TheLexer->gettok();
}

/// numberexpr ::= number
std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
  if (isnan(TheLexer->NumVal))
    return LogError("invalid double specified");
  auto Result = std::make_unique<NumberExprAST>(TheLexer->NumVal);
  getNextToken(); // consume the number
  return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
  getNextToken(); // eat (.
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ).
  return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
  std::string IdName = TheLexer->IdentifierStr;

  getNextToken(); // eat identifier.

  if (CurTok != '(') // Simple variable ref.
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken(); // eat (
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  // Eat the ')'.
  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
///   ::= ifexpr
std::unique_ptr<ExprAST> Parser::ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  case tok_if:
    return ParseIfExpr();
  case tok_for:
    return ParseForExpr();
  }
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int Parser::GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  // Make sure it's a declared binop.
  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0) return -1;
  return TokPrec;
}

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
  // If this is a binop, find its precedence.
  while (true) {
    int TokPrec = GetTokPrecedence();

    // If this is a binop that binds at least as tightly as the current binop,
    // consume it, otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    // Okay, we know this is a binop.
    int BinOp = CurTok;
    getNextToken();  // eat binop

    // Parse the primary expression after the binary operator.
    auto RHS = ParseUnary();
    if (!RHS)
        return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }
    // Merge LHS/RHS.
    LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> Parser::ParseExpression() {
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> Parser::ParsePrototype() {
  std::string FnName;
  unsigned Kind = 0;  // 0 = identifier, 1 = unary, 2 = binary.
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
    default:
      return LogErrorP("Expected function name in prototype.");
    case tok_identifier:
      FnName = TheLexer->IdentifierStr;
      getNextToken();
      break;
    case tok_unary:
      getNextToken();
      if (!isascii(CurTok))
        return LogErrorP("Expected unary operator");
      FnName = "unary";
      FnName += (char)CurTok;
      Kind = 1;
      getNextToken();
      break;
    case tok_binary:
      getNextToken();
      if (!isascii(CurTok))
        return LogErrorP("expected binary operator");
      FnName = "binary";
      FnName += (char)CurTok;
      Kind = 2;
      getNextToken();

      // Read the precedence if present.
      if (CurTok == tok_number) {
        if (TheLexer->NumVal < 1 || TheLexer->NumVal > 100)
          return LogErrorP("Invalid precedence: must be 1..100");
        BinaryPrecedence = (unsigned)TheLexer->NumVal;
        getNextToken();
      }
      break;
  }

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  // Read the list of argument names.
  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(TheLexer->IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken();  // eat ')'.

  // Verify right number of names for operator.
  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("Invalid number of operands for operator");

  auto resAst = std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0, BinaryPrecedence);
  if (resAst->IsOperator())
    AddBinop(resAst->GetOperatorName(), BinaryPrecedence);
  return std::move(resAst);
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> Parser::ParseDefinition() {
  getNextToken();  // eat def.
  auto Proto = ParsePrototype();
  if (!Proto) return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> Parser::ParseExtern() {
  getNextToken();  // eat extern.
  return ParsePrototype();
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> Parser::ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    // Make an anonymous proto.
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>(), 0, 0);
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

std::unique_ptr<ExprAST> Parser::ParseIfExpr() {
  getNextToken(); // eat if

  // parse condition
  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then) {
    return LogError("expected then");
  }
  getNextToken(); // eat then

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("expected else");

  getNextToken(); // eat else

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else));
}

std::unique_ptr<ExprAST> Parser::ParseForExpr() {
  getNextToken(); // eat for

  if (CurTok != tok_identifier)
    return LogError("expected identifier after for");
  
  std::string IdName = TheLexer->IdentifierStr;
  getNextToken(); // eat identifier

  if (CurTok != '=')
    return LogError("expected '=' after for");
  getNextToken();  // eat '='.

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' after for");
  getNextToken();  // eat 'in'.

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

std::unique_ptr<ExprAST> Parser::ParseUnary() {
  // If the current token is not an operator, it must be a primary expr.
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  // If this is a unary operator, read it.
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}