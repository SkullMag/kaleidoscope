#ifndef ERRORS_H
#define ERRORS_H

#include <memory>

#include "ast.h"

std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
llvm::Value *LogErrorV(const char *Str);

#endif