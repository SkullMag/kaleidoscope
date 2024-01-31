#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <map>

#include "ast.h"
#include "lexer.h"

class Parser {
public:
    int CurTok;

    Parser(std::unique_ptr<Lexer> lexer): TheLexer(std::move(lexer)) {}

    int getNextToken();
    int GetTokPrecedence();
    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);
    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<PrototypeAST> ParsePrototype();
    std::unique_ptr<FunctionAST> ParseDefinition();
    std::unique_ptr<PrototypeAST> ParseExtern();
    std::unique_ptr<FunctionAST> ParseTopLevelExpr();
    std::unique_ptr<ExprAST> ParseIfExpr();
    std::unique_ptr<ExprAST> ParseForExpr();
    std::unique_ptr<ExprAST> ParseUnary();
    std::unique_ptr<ExprAST> ParseVarExpr();
    void AddBinop(char op, int precedence) { BinopPrecedence[op] = precedence; }

private:
    std::unique_ptr<Lexer> TheLexer;
    /// BinopPrecedence - This holds the precedence for each binary operator that is defined.
    std::map<char, int> BinopPrecedence;
};

#endif