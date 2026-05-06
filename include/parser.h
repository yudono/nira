#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    int had_error;
    int in_args;
} Parser;

void parser_init(Parser* p, Lexer* l);
AstNode* parse_program(Parser* p);

#endif
