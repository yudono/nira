#ifndef LEXER_H
#define LEXER_H

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENT,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_EQUALS,    // =
    TOKEN_COLON,     // :
    TOKEN_LBRACE,    // {
    TOKEN_RBRACE,    // }
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_LBRACKET,  // [
    TOKEN_RBRACKET,  // ]
    TOKEN_COMMA,     // ,
    TOKEN_ARROW,     // ->
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    TOKEN_KEYWORD_IF,
    TOKEN_KEYWORD_ELSE,
    TOKEN_KEYWORD_FOR,
    TOKEN_KEYWORD_IN,
    TOKEN_KEYWORD_RETURN,
    TOKEN_KEYWORD_IMPORT,
    TOKEN_KEYWORD_EXPORT,
    TOKEN_KEYWORD_FROM,
    TOKEN_KEYWORD_OR,
    TOKEN_KEYWORD_STOP,
    TOKEN_KEYWORD_AS,
    TOKEN_KEYWORD_WHILE,
    TOKEN_KEYWORD_BREAK,
    TOKEN_KEYWORD_CONTINUE,
    TOKEN_KEYWORD_PASS,
    TOKEN_KEYWORD_AND,
    TOKEN_KEYWORD_NOT,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_KEYWORD_ERROR,
    TOKEN_KEYWORD_EXTERN,
    TOKEN_DOT,       // .
    TOKEN_OP_PLUS,
    TOKEN_OP_MINUS,
    TOKEN_OP_MUL,
    TOKEN_OP_DIV,
    TOKEN_OP_EQ,      // ==
    TOKEN_OP_NEQ,     // !=
    TOKEN_OP_LE,      // <=
    TOKEN_OP_GE,      // >=
    TOKEN_OP_LT,      // <
    TOKEN_OP_GT,      // >
    TOKEN_OP_MOD,     // %
    TOKEN_OP_POW,     // **
    TOKEN_KEYWORD_FN,
    TOKEN_KEYWORD_PRINT,
    TOKEN_KEYWORD_NATIVE,
} TokenType;

typedef struct {
    TokenType type;
    char* text;
    int length;
    int line;
    int column;
} Token;

typedef struct {
    const char* source;
    int cursor;
    int line;
    int column;
    int line_start;
    int indent_stack[64];
    int indent_level;
    int pending_dedents;
    int is_at_line_start;
} Lexer;

void lexer_init(Lexer* l, const char* source);
Token lexer_next_token(Lexer* l);
TokenType lexer_peek_n(Lexer* l, int n);
const char* token_type_to_string(TokenType type);

#endif
