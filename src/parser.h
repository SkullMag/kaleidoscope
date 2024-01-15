#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <map>

#include "ast.h"
#include "lexer.h"

class Parser {
public:
    int CurTok;

    Parser() {
        TheLexer = std::make_unique<Lexer>();
    }

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
    void AddBinop(char op, int precedence);

private:
    std::unique_ptr<Lexer> TheLexer;
    /// BinopPrecedence - This holds the precedence for each binary operator that is defined.
    std::map<char, int> BinopPrecedence;
};

#endif