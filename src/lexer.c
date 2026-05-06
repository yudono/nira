#include "../include/lexer.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void lexer_init(Lexer* l, const char* source) {
    l->source = source;
    l->cursor = 0;
    l->line = 1;
    l->line_start = 0;
    l->indent_level = 0;
    l->indent_stack[0] = 0;
    l->pending_dedents = 0;
    l->is_at_line_start = 1;
}

static char peek(Lexer* l) {
    return l->source[l->cursor];
}

static char advance(Lexer* l) {
    char c = l->source[l->cursor++];
    if (c == '\n') {
        l->line++;
        l->line_start = l->cursor;
    }
    return c;
}

static int is_at_end(Lexer* l) {
    return l->source[l->cursor] == '\0';
}

static Token make_token(Lexer* l, TokenType type, int length) {
    Token t;
    t.type = type;
    t.text = (char*)&l->source[l->cursor - length];
    t.length = length;
    t.line = l->line;
    t.column = (l->cursor - length) - l->line_start + 1;
    return t;
}

static TokenType check_keyword(const char* text, int len) {
    if (strncmp(text, "if", len) == 0 && len == 2) return TOKEN_KEYWORD_IF;
    if (strncmp(text, "else", len) == 0 && len == 4) return TOKEN_KEYWORD_ELSE;
    if (strncmp(text, "for", len) == 0 && len == 3) return TOKEN_KEYWORD_FOR;
    if (strncmp(text, "in", len) == 0 && len == 2) return TOKEN_KEYWORD_IN;
    if (strncmp(text, "return", len) == 0 && len == 6) return TOKEN_KEYWORD_RETURN;
    if (strncmp(text, "import", len) == 0 && len == 6) return TOKEN_KEYWORD_IMPORT;
    if (strncmp(text, "export", len) == 0 && len == 6) return TOKEN_KEYWORD_EXPORT;
    if (strncmp(text, "from", len) == 0 && len == 4) return TOKEN_KEYWORD_FROM;
    if (strncmp(text, "or", len) == 0 && len == 2) return TOKEN_KEYWORD_OR;
    if (strncmp(text, "stop", len) == 0 && len == 4) return TOKEN_KEYWORD_STOP;
    if (strncmp(text, "as", len) == 0 && len == 2) return TOKEN_KEYWORD_AS;
    if (strncmp(text, "while", len) == 0 && len == 5) return TOKEN_KEYWORD_WHILE;
    if (strncmp(text, "break", len) == 0 && len == 5) return TOKEN_KEYWORD_BREAK;
    if (strncmp(text, "continue", len) == 0 && len == 8) return TOKEN_KEYWORD_CONTINUE;
    if (strncmp(text, "pass", len) == 0 && len == 4) return TOKEN_KEYWORD_PASS;
    if (strncmp(text, "and", len) == 0 && len == 3) return TOKEN_KEYWORD_AND;
    if (strncmp(text, "not", len) == 0 && len == 3) return TOKEN_KEYWORD_NOT;
    if (strncmp(text, "true", len) == 0 && len == 4) return TOKEN_TRUE;
    if (strncmp(text, "false", len) == 0 && len == 5) return TOKEN_FALSE;
    if (strncmp(text, "null", len) == 0 && len == 4) return TOKEN_NULL;
    if (strncmp(text, "error", len) == 0 && len == 5) return TOKEN_KEYWORD_ERROR;
    if (strncmp(text, "fn", len) == 0 && len == 2) return TOKEN_KEYWORD_FN;
    return TOKEN_IDENT;
}

Token lexer_next_token(Lexer* l) {
    if (is_at_end(l)) {
        if (l->indent_level > 0) {
            l->pending_dedents = l->indent_level;
            l->indent_level = 0;
            return lexer_next_token(l);
        }
        return make_token(l, TOKEN_EOF, 0);
    }

    // 1. Handle pending dedents from previous indentation change
    if (l->pending_dedents > 0) {
        l->pending_dedents--;
        return make_token(l, TOKEN_DEDENT, 0);
    }

    // 2. Handle Indentation at line start
    if (l->is_at_line_start) {
        l->is_at_line_start = 0;
        int spaces = 0;
        while (peek(l) == ' ') {
            advance(l);
            spaces++;
        }

        // Ignore empty lines or comment-only lines
        if (peek(l) == '\n' || peek(l) == '\0' || peek(l) == '\r') {
            l->is_at_line_start = 1;
            if (peek(l) == '\n') {
                advance(l);
            } else if (peek(l) == '\r') {
                advance(l);
            }
            if (peek(l) == '\0') return lexer_next_token(l); // Let the EOF check handle it
            return lexer_next_token(l);
        }

        int current_indent = l->indent_stack[l->indent_level];
        if (spaces > current_indent) {
            l->indent_level++;
            l->indent_stack[l->indent_level] = spaces;
            return make_token(l, TOKEN_INDENT, 0);
        } else if (spaces < current_indent) {
            while (l->indent_level > 0 && l->indent_stack[l->indent_level] > spaces) {
                l->indent_level--;
                l->pending_dedents++;
            }
            if (l->indent_stack[l->indent_level] != spaces) {
                fprintf(stderr, "Indentation error at line %d\n", l->line);
            }
            if (l->pending_dedents > 0) {
                l->pending_dedents--;
                return make_token(l, TOKEN_DEDENT, 0);
            }
        }
    }

    // Skip whitespace and comments
    while (1) {
        char c = peek(l);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance(l);
        } else if (c == '#') {
            while (peek(l) != '\n' && peek(l) != '\0') {
                advance(l);
            }
        } else {
            break;
        }
    }

    char c = advance(l);

    // Identifiers and Keywords
    if (isalpha(c) || c == '_') {
        int start = l->cursor - 1;
        while (isalnum(peek(l)) || peek(l) == '_') advance(l);
        int len = l->cursor - start;
        return make_token(l, check_keyword(&l->source[start], len), len);
    }

    // Numbers
    if (isdigit(c)) {
        int start = l->cursor - 1;
        while (isdigit(peek(l))) advance(l);
        return make_token(l, TOKEN_NUMBER, l->cursor - start);
    }

    // Strings
    if (c == '"') {
        int start = l->cursor - 1;
        while (peek(l) != '"' && !is_at_end(l)) advance(l);
        if (is_at_end(l)) {
            fprintf(stderr, "Unterminated string at line %d\n", l->line);
        } else {
            advance(l); // closing quote
        }
        return make_token(l, TOKEN_STRING, l->cursor - start);
    }

    // Operators and Punctuation
    switch (c) {
        case '=':
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOKEN_OP_EQ, 2);
            }
            return make_token(l, TOKEN_EQUALS, 1);
        case ':': return make_token(l, TOKEN_COLON, 1);
        case '!':
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOKEN_OP_NEQ, 2);
            }
            break; // Unexpected character '!' handled below
        case '.': return make_token(l, TOKEN_DOT, 1);
        case '{': return make_token(l, TOKEN_LBRACE, 1);
        case '}': return make_token(l, TOKEN_RBRACE, 1);
        case '(': return make_token(l, TOKEN_LPAREN, 1);
        case ')': return make_token(l, TOKEN_RPAREN, 1);
        case '[': return make_token(l, TOKEN_LBRACKET, 1);
        case ']': return make_token(l, TOKEN_RBRACKET, 1);
        case ',': return make_token(l, TOKEN_COMMA, 1);
        case '-':
            if (peek(l) == '>') {
                advance(l);
                return make_token(l, TOKEN_ARROW, 2);
            }
            return make_token(l, TOKEN_OP_MINUS, 1);
        case '+': return make_token(l, TOKEN_OP_PLUS, 1);
        case '*': return make_token(l, TOKEN_OP_MUL, 1);
        case '/': return make_token(l, TOKEN_OP_DIV, 1);
        case '<':
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOKEN_OP_LE, 2);
            }
            return make_token(l, TOKEN_OP_LT, 1);
        case '>':
            if (peek(l) == '=') {
                advance(l);
                return make_token(l, TOKEN_OP_GE, 2);
            }
            return make_token(l, TOKEN_OP_GT, 1);
        case '\n':
            l->is_at_line_start = 1;
            return make_token(l, TOKEN_NEWLINE, 1);
    }

    fprintf(stderr, "Unexpected character '%c' at line %d\n", c, l->line);
    return lexer_next_token(l); // skip and continue
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_NUMBER: return "NUMBER";
        case TOKEN_STRING: return "STRING";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_COLON: return "COLON";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        case TOKEN_KEYWORD_IF: return "IF";
        case TOKEN_KEYWORD_ELSE: return "ELSE";
        case TOKEN_KEYWORD_FOR: return "FOR";
        case TOKEN_KEYWORD_IN: return "IN";
        case TOKEN_KEYWORD_RETURN: return "RETURN";
        case TOKEN_KEYWORD_IMPORT: return "IMPORT";
        case TOKEN_KEYWORD_EXPORT: return "EXPORT";
        case TOKEN_KEYWORD_FROM: return "FROM";
        case TOKEN_KEYWORD_OR: return "OR";
        case TOKEN_KEYWORD_STOP: return "STOP";
        case TOKEN_KEYWORD_AS: return "AS";
        case TOKEN_KEYWORD_WHILE: return "WHILE";
        case TOKEN_KEYWORD_BREAK: return "BREAK";
        case TOKEN_KEYWORD_CONTINUE: return "CONTINUE";
        case TOKEN_KEYWORD_PASS: return "PASS";
        case TOKEN_KEYWORD_AND: return "AND";
        case TOKEN_KEYWORD_NOT: return "NOT";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_NULL: return "NULL";
        case TOKEN_KEYWORD_ERROR: return "ERROR";
        case TOKEN_KEYWORD_FN: return "FN";
        case TOKEN_DOT: return "DOT";
        case TOKEN_OP_PLUS: return "PLUS";
        case TOKEN_OP_MINUS: return "MINUS";
        case TOKEN_OP_MUL: return "MUL";
        case TOKEN_OP_DIV: return "DIV";
        case TOKEN_OP_EQ: return "EQ";
        case TOKEN_OP_NEQ: return "NEQ";
        case TOKEN_OP_LE: return "LE";
        case TOKEN_OP_GE: return "GE";
        case TOKEN_OP_LT: return "LT";
        case TOKEN_OP_GT: return "GT";
        default: return "UNKNOWN";
    }
}

TokenType lexer_peek_n(Lexer* l, int n) {
    Lexer save = *l;
    Token t = {0};
    for (int i = 0; i < n; i++) {
        t = lexer_next_token(l);
        if (t.type == TOKEN_EOF) break;
    }
    *l = save;
    return t.type;
}
