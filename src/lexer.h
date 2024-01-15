#ifndef LEXER_H
#define LEXER_H

#include <string>

class Lexer {
public:
    int gettok();
    std::string IdentifierStr;
    double NumVal;
};

enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5
};

#endif