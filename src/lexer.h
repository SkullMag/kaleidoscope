#ifndef LEXER_H
#define LEXER_H

#include <string>

class Lexer {
public:
    int gettok();
    std::string IdentifierStr;
    double NumVal;
};

#endif