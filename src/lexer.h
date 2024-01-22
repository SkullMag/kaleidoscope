#ifndef LEXER_H
#define LEXER_H

#include <string>

class Lexer {
public:
    std::string IdentifierStr;
    double NumVal;

    int gettok();
};

#endif