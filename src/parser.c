#include "../include/parser.h"
#include "../include/arena.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

static void advance(Parser* p) {
    p->previous = p->current;
    p->current = lexer_next_token(p->lexer);
}

static int check(Parser* p, TokenType type) {
    return p->current.type == type;
}

static int match(Parser* p, TokenType type) {
    if (check(p, type)) {
        advance(p);
        return 1;
    }
    return 0;
}

static void report_error(Parser* p, Token tok, const char* msg) {
    if (p->had_error) return;
    fprintf(stderr, "\n\033[1;31m[SYNTAX ERROR]\033[0m %s\n", msg);
    fprintf(stderr, "\033[1;34m-->\033[0m source.nr:%d:%d\n", tok.line, tok.column);
    
    if (tok.text && p->lexer && p->lexer->source) {
        const char* start = tok.text;
        while (start > p->lexer->source && *(start-1) != '\n') start--;
        const char* end = tok.text;
        while (*end != '\n' && *end != '\0') end++;

        int line_len = end - start;
        fprintf(stderr, " \033[1;34m|\033[0m\n");
        fprintf(stderr, " \033[1;34m| \033[0m %.*s\n", line_len, start);
        fprintf(stderr, " \033[1;34m| \033[1;31m");
        for (int i = 1; i < tok.column; i++) fprintf(stderr, " ");
        for (int i = 0; i < tok.length; i++) fprintf(stderr, "^");
        fprintf(stderr, "\033[0m\n");
        fprintf(stderr, " \033[1;34m|\033[0m\n\n");
    }
    
    p->had_error = 1;
}

static void consume(Parser* p, TokenType type, const char* msg) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    char buf[256];
    snprintf(buf, sizeof(buf), "%s (got %s)", msg, token_type_to_string(p->current.type));
    report_error(p, p->current, buf);
}

static char* copy_token_text(Token t) {
    char* s = nr_malloc(t.length + 1);
    memcpy(s, t.text, t.length);
    s[t.length] = '\0';
    return s;
}

static char* strip_quotes(Token t) {
    if (t.length < 2) return nr_strdup("");
    char* s = nr_malloc(t.length - 1);
    memcpy(s, t.text + 1, t.length - 2);
    s[t.length - 2] = '\0';
    return s;
}

void parser_init(Parser* p, Lexer* l) {
    p->lexer = l;
    p->had_error = 0;
    p->in_args = 0;
    advance(p);
}

// Forward declarations
static AstNode* parse_expression(Parser* p);
static AstNode* parse_statement(Parser* p);

static AstNode* parse_object(Parser* p) {
    AstNode* node = ast_new(AST_OBJECT, p->current);
    AstField* last = NULL;

    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        if (match(p, TOKEN_NEWLINE) || match(p, TOKEN_INDENT) || match(p, TOKEN_DEDENT)) continue;

        if (!check(p, TOKEN_IDENT) && !check(p, TOKEN_STRING)) {
             report_error(p, p->current, "Expect field name in object");
             break;
        }
        
        advance(p);
        char* name = copy_token_text(p->previous);
        if (p->previous.type == TOKEN_STRING) {
            // Strip quotes
            int len = strlen(name);
            memmove(name, name + 1, len - 2);
            name[len - 2] = '\0';
        }
        char* alias = NULL;
        if (match(p, TOKEN_KEYWORD_AS)) {
            consume(p, TOKEN_IDENT, "Expect alias name after 'as'");
            alias = copy_token_text(p->previous);
        }

        AstNode* value;
        if (match(p, TOKEN_COLON)) {
            value = parse_expression(p);
        } else {
            value = ast_new(AST_VAR_REF, p->previous);
            value->data.var_name = strdup(name);
        }

        AstField* field = nr_malloc(sizeof(AstField));
        field->name = name;
        field->value = value;
        field->alias = alias;
        field->next = NULL;

        if (last) last->next = field;
        else node->data.object.fields = field;
        last = field;

        if (match(p, TOKEN_COMMA)) {}
    }
    consume(p, TOKEN_RBRACE, "Expect '}' after object");
    return node;
}

static AstNode* parse_array(Parser* p) {
    AstNode* node = ast_new(AST_ARRAY, p->current);
    int capacity = 8;
    int count = 0;
    AstNode** elements = nr_malloc(sizeof(AstNode*) * capacity);

    while (!check(p, TOKEN_RBRACKET) && !check(p, TOKEN_EOF)) {
        if (match(p, TOKEN_NEWLINE) || match(p, TOKEN_INDENT) || match(p, TOKEN_DEDENT)) continue;
        
        if (count >= capacity) {
            int old_cap = capacity;
            capacity *= 2;
            elements = nr_realloc(elements, sizeof(AstNode*) * old_cap, sizeof(AstNode*) * capacity);
        }
        elements[count++] = parse_expression(p);
        if (match(p, TOKEN_COMMA)) {}
    }
    consume(p, TOKEN_RBRACKET, "Expect ']' after array");
    node->data.array.elements = elements;
    node->data.array.count = count;
    return node;
}

static AstNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER)) {
        bool is_float = false;
        for (int i = 0; i < p->previous.length; i++) {
            if (p->previous.text[i] == '.') {
                is_float = true;
                break;
            }
        }
        if (is_float) {
            AstNode* node = ast_new(AST_LITERAL_FLOAT, p->previous);
            node->data.float_val = strtod(p->previous.text, NULL);
            return node;
        } else {
            AstNode* node = ast_new(AST_LITERAL_INT, p->previous);
            node->data.int_val = (int)strtol(p->previous.text, NULL, 10);
            return node;
        }
    }
    if (match(p, TOKEN_STRING)) {
        AstNode* node = ast_new(AST_LITERAL_STR, p->previous);
        node->data.str_val = strip_quotes(p->previous);
        return node;
    }
    if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE)) {
        AstNode* node = ast_new(AST_LITERAL_BOOL, p->previous);
        node->data.int_val = (p->previous.type == TOKEN_TRUE);
        return node;
    }
    if (match(p, TOKEN_NULL)) {
        return ast_new(AST_LITERAL_NULL, p->previous);
    }
    if (match(p, TOKEN_KEYWORD_ERROR)) {
        AstNode* node = ast_new(AST_ERROR, p->current);
        node->data.error_expr.message = parse_expression(p);
        return node;
    }
    if (match(p, TOKEN_LBRACE)) {
        return parse_object(p);
    }
    if (match(p, TOKEN_LBRACKET)) {
        return parse_array(p);
    }
    if (match(p, TOKEN_KEYWORD_FN)) {
        AstNode* node = ast_new(AST_FUNC_DECL, p->current);
        node->data.func_decl.name = strdup("anonymous");
        
        int capacity = 8;
        int count = 0;
        char** params = nr_malloc(sizeof(char*) * capacity);

        if (check(p, TOKEN_LPAREN)) {
            report_error(p, p->current, "Anonymous functions should not use parentheses. Use 'fn p1 p2:' instead.");
            return NULL;
        }

        // Command-style parameters: fn p1 p2:
        while (check(p, TOKEN_IDENT)) {
            advance(p);
            if (count >= capacity) {
                int old_cap = capacity;
                capacity *= 2;
                params = nr_realloc(params, sizeof(char*) * old_cap, sizeof(char*) * capacity);
            }
            params[count++] = copy_token_text(p->previous);
            if (match(p, TOKEN_COMMA)) {}
        }
        
        node->data.func_decl.params = params;
        node->data.func_decl.param_count = count;
        
        consume(p, TOKEN_COLON, "Expect ':' after anonymous function signature");
        while (match(p, TOKEN_NEWLINE)) ;
        consume(p, TOKEN_INDENT, "Expect indentation after ':'");
        node->data.func_decl.body = parse_program(p);
        if (check(p, TOKEN_DEDENT)) advance(p);
        return node;
    }
    if (match(p, TOKEN_LPAREN)) {
        int capacity = 8;
        int count = 0;
        AstNode** args = nr_malloc(sizeof(AstNode*) * capacity);
        if (!check(p, TOKEN_RPAREN)) {
            do {
                if (count >= capacity) {
                    int old_cap = capacity;
                    capacity *= 2;
                    args = nr_realloc(args, sizeof(AstNode*) * old_cap, sizeof(AstNode*) * capacity);
                }
                args[count++] = parse_expression(p);
            } while (match(p, TOKEN_COMMA));
        }
        consume(p, TOKEN_RPAREN, "Expect ')' after expression");

        if (match(p, TOKEN_ARROW)) {
            char** params = malloc(sizeof(char*) * count);
            for (int i=0; i<count; i++) {
                if (args[i]->type != AST_VAR_REF) {
                    report_error(p, p->previous, "Lambda parameters must be identifiers");
                } else {
                    params[i] = strdup(args[i]->data.var_name);
                }
            }
            AstNode* lambda = ast_new(AST_FUNC_DECL, p->current);
            lambda->data.func_decl.name = strdup("anonymous");
            lambda->data.func_decl.params = params;
            lambda->data.func_decl.param_count = count;
            lambda->data.func_decl.body = parse_expression(p);
            return lambda;
        }

        if (count == 1) return args[0];
        // If count > 1 and no arrow, it's an error in this context
        report_error(p, p->previous, "Unexpected comma in expression");
        return args[0];
    }
    if (match(p, TOKEN_IDENT)) {
        AstNode* node = ast_new(AST_VAR_REF, p->current);
        node->data.var_name = copy_token_text(p->previous);
        
        while (1) {
            if (match(p, TOKEN_DOT)) {
                consume(p, TOKEN_IDENT, "Expect identifier after '.'");
                if (node->type == AST_VAR_REF) {
                    int old_len = strlen(node->data.var_name);
                    int add_len = p->previous.length + 1;
                    node->data.var_name = realloc(node->data.var_name, old_len + add_len + 1);
                    strcat(node->data.var_name, ".");
                    strncat(node->data.var_name, p->previous.text, p->previous.length);
                } else {
                    // It's an AST_INDEX followed by a DOT. We can treat it as another AST_INDEX where index is a string literal!
                    AstNode* index_node = ast_new(AST_INDEX, p->current);
                    index_node->data.index.object = node;
                    AstNode* str_node = ast_new(AST_LITERAL_STR, p->current);
                    str_node->data.str_val = copy_token_text(p->previous);
                    index_node->data.index.index = str_node;
                    node = index_node;
                }
            } else if (match(p, TOKEN_LBRACKET)) {
                AstNode* index_expr = parse_expression(p);
                consume(p, TOKEN_RBRACKET, "Expect ']' after index");
                AstNode* index_node = ast_new(AST_INDEX, p->current);
                index_node->data.index.object = node;
                index_node->data.index.index = index_expr;
                node = index_node;
            } else {
                break;
            }
        }

        if (match(p, TOKEN_ARROW)) {
            char** params = malloc(sizeof(char*));
            params[0] = node->data.var_name;
            AstNode* lambda = ast_new(AST_FUNC_DECL, p->current);
            lambda->data.func_decl.name = strdup("anonymous");
            lambda->data.func_decl.params = params;
            lambda->data.func_decl.param_count = 1;
            lambda->data.func_decl.body = parse_expression(p);
            return lambda;
        }

        if (match(p, TOKEN_LPAREN)) {
            AstNode* call = ast_new(AST_CALL, p->current);
            if (node->type == AST_VAR_REF) {
                call->data.call.name = node->data.var_name;
            } else {
                call->data.call.name = strdup("anonymous"); // Or handle function pointers later
            }
            int capacity = 8;
            int count = 0;
            AstNode** args = nr_malloc(sizeof(AstNode*) * capacity);
            while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
                if (count >= capacity) {
                    int old_cap = capacity;
                    capacity *= 2;
                    args = nr_realloc(args, sizeof(AstNode*) * old_cap, sizeof(AstNode*) * capacity);
                }
                AstNode* arg_expr = parse_expression(p);
                if (!arg_expr) {
                    advance(p); // PREVENT INFINITE LOOP
                } else {
                    args[count++] = arg_expr;
                }
                if (match(p, TOKEN_COMMA)) {}
            }
            consume(p, TOKEN_RPAREN, "Expect ')' after arguments");
            
            if (match(p, TOKEN_ARROW)) {
                // (p1, p2) -> body
                char** params = malloc(sizeof(char*) * count);
                for (int i=0; i<count; i++) {
                    if (args[i]->type != AST_VAR_REF) {
                        report_error(p, p->previous, "Lambda parameters must be identifiers");
                    } else {
                        params[i] = strdup(args[i]->data.var_name);
                    }
                }
                AstNode* lambda = ast_new(AST_FUNC_DECL, p->current);
                lambda->data.func_decl.name = strdup("anonymous");
                lambda->data.func_decl.params = params;
                lambda->data.func_decl.param_count = count;
                lambda->data.func_decl.body = parse_expression(p);
                return lambda;
            }
            
            call->data.call.args = args;
            call->data.call.arg_count = count;
            return call;
        }

        return node;
    }
    return NULL;
}

static AstNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_KEYWORD_NOT)) {
        AstNode* node = ast_new(AST_BINARY, p->current);
        node->data.binary.op = strdup("not");
        node->data.binary.left = parse_unary(p);
        node->data.binary.right = NULL;
        return node;
    }
    if (match(p, TOKEN_OP_MINUS)) {
        AstNode* node = ast_new(AST_BINARY, p->current);
        node->data.binary.op = strdup("-");
        AstNode* zero = ast_new(AST_LITERAL_INT, p->current);
        zero->data.int_val = 0;
        node->data.binary.left = zero;
        node->data.binary.right = parse_unary(p);
        return node;
    }
    return parse_primary(p);
}

static AstNode* parse_multiplication(Parser* p) {
    AstNode* expr = parse_unary(p);
    while (match(p, TOKEN_OP_MUL) || match(p, TOKEN_OP_DIV)) {
        char* op = copy_token_text(p->previous);
        AstNode* right = parse_unary(p);
        AstNode* binary = ast_new(AST_BINARY, p->current);
        binary->data.binary.left = expr;
        binary->data.binary.op = op;
        binary->data.binary.right = right;
        expr = binary;
    }
    return expr;
}

static AstNode* parse_addition(Parser* p) {
    AstNode* expr = parse_multiplication(p);
    while (match(p, TOKEN_OP_PLUS) || match(p, TOKEN_OP_MINUS)) {
        char* op = copy_token_text(p->previous);
        AstNode* right = parse_multiplication(p);
        AstNode* binary = ast_new(AST_BINARY, p->current);
        binary->data.binary.left = expr;
        binary->data.binary.op = op;
        binary->data.binary.right = right;
        expr = binary;
    }
    return expr;
}

static AstNode* parse_comparison(Parser* p) {
    AstNode* expr = parse_addition(p);
    while (match(p, TOKEN_OP_LT) || match(p, TOKEN_OP_GT) || match(p, TOKEN_OP_LE) || match(p, TOKEN_OP_GE)) {
        char* op = copy_token_text(p->previous);
        AstNode* right = parse_addition(p);
        AstNode* binary = ast_new(AST_BINARY, p->current);
        binary->data.binary.left = expr;
        binary->data.binary.op = op;
        binary->data.binary.right = right;
        expr = binary;
    }
    return expr;
}

static AstNode* parse_equality(Parser* p) {
    AstNode* expr = parse_comparison(p);
    while (match(p, TOKEN_OP_EQ) || match(p, TOKEN_OP_NEQ)) {
        char* op = copy_token_text(p->previous);
        AstNode* right = parse_comparison(p);
        AstNode* binary = ast_new(AST_BINARY, p->current);
        binary->data.binary.left = expr;
        binary->data.binary.op = op;
        binary->data.binary.right = right;
        expr = binary;
    }
    return expr;
}

static AstNode* parse_logical(Parser* p) {
    AstNode* expr = parse_equality(p);
    while (match(p, TOKEN_KEYWORD_AND) || match(p, TOKEN_KEYWORD_OR)) {
        char* op = copy_token_text(p->previous);
        AstNode* right = parse_equality(p);
        AstNode* binary = ast_new(AST_BINARY, p->current);
        binary->data.binary.left = expr;
        binary->data.binary.op = op;
        binary->data.binary.right = right;
        expr = binary;
    }
    return expr;
}

static AstNode* parse_native_stmt(Parser* p) {
    AstNode* node = ast_new(AST_NATIVE, p->current);
    consume(p, TOKEN_COLON, "Expect ':' after native");
    while (match(p, TOKEN_NEWLINE)) ;
    consume(p, TOKEN_INDENT, "Expect indent after native");

    int link_cap = 4;
    node->data.native_stmt.links = nr_malloc(sizeof(char*) * link_cap);
    node->data.native_stmt.link_count = 0;

    int head_cap = 4;
    node->data.native_stmt.headers = nr_malloc(sizeof(char*) * head_cap);
    node->data.native_stmt.header_count = 0;
    node->data.native_stmt.code = NULL;

    while (!check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) {
        if (match(p, TOKEN_NEWLINE)) continue;
        if (match(p, TOKEN_IDENT)) {
            char* cmd = copy_token_text(p->previous);
            if (strcmp(cmd, "link") == 0) {
                consume(p, TOKEN_STRING, "Expect string after 'link'");
                if (node->data.native_stmt.link_count >= link_cap) {
                    node->data.native_stmt.links = nr_realloc(node->data.native_stmt.links, sizeof(char*) * link_cap, sizeof(char*) * link_cap * 2);
                    link_cap *= 2;
                }
                node->data.native_stmt.links[node->data.native_stmt.link_count++] = strip_quotes(p->previous);
            } else if (strcmp(cmd, "header") == 0) {
                consume(p, TOKEN_STRING, "Expect string after 'header'");
                if (node->data.native_stmt.header_count >= head_cap) {
                    node->data.native_stmt.headers = nr_realloc(node->data.native_stmt.headers, sizeof(char*) * head_cap, sizeof(char*) * head_cap * 2);
                    head_cap *= 2;
                }
                node->data.native_stmt.headers[node->data.native_stmt.header_count++] = strip_quotes(p->previous);
            } else if (strcmp(cmd, "code") == 0) {
                consume(p, TOKEN_COLON, "Expect ':' after 'code'");
                consume(p, TOKEN_STRING, "Expect string after 'code:'");
                node->data.native_stmt.code = strip_quotes(p->previous);
            }
        } else {
            advance(p); // Skip unknown tokens in native block
        }
    }
    if (check(p, TOKEN_DEDENT)) advance(p);
    return node;
}

static AstNode* parse_expression(Parser* p) {
    return parse_logical(p);
}

static AstNode* parse_statement(Parser* p) {
    while (match(p, TOKEN_NEWLINE)) ;

    if (match(p, TOKEN_KEYWORD_NATIVE)) {
        return parse_native_stmt(p);
    }

    if (match(p, TOKEN_KEYWORD_PASS)) {
        return ast_new(AST_PASS, p->current);
    }
    if (match(p, TOKEN_KEYWORD_BREAK)) {
        return ast_new(AST_BREAK, p->current);
    }
    if (match(p, TOKEN_KEYWORD_CONTINUE)) {
        return ast_new(AST_CONTINUE, p->current);
    }

    // Removed special print handling to treat it as normal function call

    if (match(p, TOKEN_KEYWORD_RETURN)) {
        AstNode* node = ast_new(AST_RETURN, p->current);
        node->data.ret.value = parse_expression(p);
        return node;
    }

    if (match(p, TOKEN_KEYWORD_IF)) {
        AstNode* node = ast_new(AST_IF, p->current);
        node->data.if_stmt.condition = parse_expression(p);
        consume(p, TOKEN_COLON, "Expect ':' after if condition");
        while (match(p, TOKEN_NEWLINE)) ;
        consume(p, TOKEN_INDENT, "Expect indent after if");
        node->data.if_stmt.then_branch = parse_program(p);
        if (check(p, TOKEN_DEDENT)) advance(p);
        
        if (match(p, TOKEN_KEYWORD_ELSE)) {
            consume(p, TOKEN_COLON, "Expect ':' after else");
            while (match(p, TOKEN_NEWLINE)) ;
            consume(p, TOKEN_INDENT, "Expect indent after else");
            node->data.if_stmt.else_branch = parse_program(p);
            if (check(p, TOKEN_DEDENT)) advance(p);
        }
        return node;
    }

    if (match(p, TOKEN_KEYWORD_WHILE)) {
        AstNode* node = ast_new(AST_WHILE, p->current);
        node->data.while_stmt.condition = parse_expression(p);
        consume(p, TOKEN_COLON, "Expect ':' after while condition");
        while (match(p, TOKEN_NEWLINE)) ;
        consume(p, TOKEN_INDENT, "Expect indent after while");
        node->data.while_stmt.body = parse_program(p);
        if (check(p, TOKEN_DEDENT)) advance(p);
        return node;
    }

    if (match(p, TOKEN_KEYWORD_FOR)) {
        AstNode* node = ast_new(AST_FOR, p->current);
        consume(p, TOKEN_IDENT, "Expect variable name in for loop");
        node->data.for_stmt.var = copy_token_text(p->previous);
        
        if (match(p, TOKEN_KEYWORD_AS)) {
            consume(p, TOKEN_IDENT, "Expect alias name after 'as'");
            node->data.for_stmt.alias = copy_token_text(p->previous);
        }

        consume(p, TOKEN_KEYWORD_IN, "Expect 'in' after variable in for loop");
        node->data.for_stmt.iterable = parse_expression(p);
        consume(p, TOKEN_COLON, "Expect ':' after for loop");
        while (match(p, TOKEN_NEWLINE)) ;
        consume(p, TOKEN_INDENT, "Expect indent after for");
        node->data.for_stmt.body = parse_program(p);
        if (check(p, TOKEN_DEDENT)) advance(p);
        return node;
    }

    if (match(p, TOKEN_KEYWORD_FROM)) {
        // from module import { symbols }
        consume(p, TOKEN_IDENT, "Expect module name after 'from'");
        char* path = copy_token_text(p->previous);
        consume(p, TOKEN_KEYWORD_IMPORT, "Expect 'import' after module name");
        
        char* symbols[32];
        int count = 0;
        if (match(p, TOKEN_LBRACE)) {
            while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
                consume(p, TOKEN_IDENT, "Expect symbol name");
                symbols[count++] = copy_token_text(p->previous);
                if (match(p, TOKEN_COMMA)) {}
            }
            consume(p, TOKEN_RBRACE, "Expect '}' after symbols");
        } else {
            consume(p, TOKEN_IDENT, "Expect symbol name");
            symbols[count++] = copy_token_text(p->previous);
            while (match(p, TOKEN_COMMA)) {
                consume(p, TOKEN_IDENT, "Expect symbol name");
                symbols[count++] = copy_token_text(p->previous);
            }
        }

        AstNode* node = ast_new(AST_IMPORT, p->previous);
        node->data.import_stmt.path = path;
        node->data.import_stmt.symbol_count = count;
        if (count > 0) {
            node->data.import_stmt.symbols = malloc(sizeof(char*) * count);
            memcpy(node->data.import_stmt.symbols, symbols, sizeof(char*) * count);
        }
        return node;
    }

    if (match(p, TOKEN_KEYWORD_IMPORT)) {
        // import module
        // import { symbols } from module
        // import symbol from module
        
        char* symbols[32];
        int count = 0;
        int has_from = 0;

        if (check(p, TOKEN_LBRACE)) {
            advance(p);
            while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
                consume(p, TOKEN_IDENT, "Expect symbol name");
                symbols[count++] = copy_token_text(p->previous);
                if (match(p, TOKEN_COMMA)) {}
            }
            consume(p, TOKEN_RBRACE, "Expect '}' after symbols");
            consume(p, TOKEN_KEYWORD_FROM, "Expect 'from' after symbols");
            has_from = 1;
        } else {
            // Check if it's 'import now from time'
            if (lexer_peek_n(p->lexer, 1) == TOKEN_KEYWORD_FROM) {
                consume(p, TOKEN_IDENT, "Expect symbol name");
                symbols[count++] = copy_token_text(p->previous);
                consume(p, TOKEN_KEYWORD_FROM, "Expect 'from' after symbol");
                has_from = 1;
            }
        }

        char* path;
        if (match(p, TOKEN_STRING)) {
            path = strip_quotes(p->previous);
        } else {
            consume(p, TOKEN_IDENT, "Expect module name or string path");
            path = copy_token_text(p->previous);
        }

        AstNode* node = ast_new(AST_IMPORT, p->previous);
        node->data.import_stmt.path = path;
        node->data.import_stmt.symbol_count = count;
        if (count > 0) {
            node->data.import_stmt.symbols = malloc(sizeof(char*) * count);
            memcpy(node->data.import_stmt.symbols, symbols, sizeof(char*) * count);
        }

        if (match(p, TOKEN_KEYWORD_AS)) {
            consume(p, TOKEN_IDENT, "Expect alias name after 'as'");
            node->data.import_stmt.alias = copy_token_text(p->previous);
        }

        return node;
    }

    int is_exported = 0;
    if (match(p, TOKEN_KEYWORD_EXPORT)) is_exported = 1;

    if (check(p, TOKEN_IDENT)) {
        int i = p->lexer->cursor;
        const char* s = p->lexer->source;
        while (isalnum(s[i]) || s[i] == '_') i++; // Skip name
        while (s[i] == ' ' || s[i] == '\t') i++;
        
        // Skip possible parameters
        while (isalpha(s[i]) || s[i] == '_') {
            while (isalnum(s[i]) || s[i] == '_') i++;
            while (s[i] == ' ' || s[i] == '\t' || s[i] == ',') i++;
        }
        
        if (s[i] == '(') {
            int depth = 0;
            int j = i;
            do {
                if (s[j] == '(') depth++;
                else if (s[j] == ')') depth--;
                j++;
            } while (s[j] != '\0' && depth > 0);
            while (s[j] == ' ' || s[j] == '\t') j++;
            if (s[j] == ':') {
                i = j; // Found it!
            }
        }
        
        if (s[i] == ':') {
            advance(p);
            AstNode* node = ast_new(AST_FUNC_DECL, p->current);
            node->data.func_decl.name = copy_token_text(p->previous);
            node->data.func_decl.is_exported = is_exported;
            node->data.func_decl.is_unpacking = 0;
            
            int capacity = 8;
            int count = 0;
            char** params = nr_malloc(sizeof(char*) * capacity);
            
            if (check(p, TOKEN_LPAREN)) {
                report_error(p, p->current, "Function declarations should not use parentheses. Use 'name param1 param2:' instead.");
                return NULL;
            }

            // Command-style parameters: IDENT p1 p2:
            while (check(p, TOKEN_IDENT)) {
                advance(p);
                if (count >= capacity) {
                    int old_cap = capacity;
                    capacity *= 2;
                    params = nr_realloc(params, sizeof(char*) * old_cap, sizeof(char*) * capacity);
                }
                params[count++] = copy_token_text(p->previous);
                if (match(p, TOKEN_COMMA)) {}
            }
            
            node->data.func_decl.params = params;
            node->data.func_decl.param_count = count;
            
            consume(p, TOKEN_COLON, "Expect ':' after function signature");
            while (match(p, TOKEN_NEWLINE)) ; 
            consume(p, TOKEN_INDENT, "Expect indentation after ':'");
            node->data.func_decl.body = parse_program(p);
            if (check(p, TOKEN_DEDENT)) advance(p);
            return node;
        }
    }

    AstNode* expr = parse_expression(p);
    if (match(p, TOKEN_EQUALS)) {
        if (expr->type == AST_VAR_REF) {
            AstNode* assign = ast_new(AST_ASSIGN, p->current);
            assign->data.assign.target = expr->data.var_name;
            assign->data.assign.value = parse_expression(p);
            return assign;
        } else if (expr->type == AST_INDEX) {
            AstNode* assign = ast_new(AST_INDEX_ASSIGN, p->current);
            assign->data.index_assign.object = expr->data.index.object;
            assign->data.index_assign.index = expr->data.index.index;
            assign->data.index_assign.value = parse_expression(p);
            return assign;
        } else if (expr->type == AST_OBJECT) {
            AstNode* assign = ast_new(AST_DESTRUCTURING, p->current);
            assign->data.destruct.target = expr;
            assign->data.destruct.value = parse_expression(p);
            return assign;
        }
    }
    if (expr && expr->type == AST_VAR_REF && match(p, TOKEN_KEYWORD_AS)) {
         consume(p, TOKEN_IDENT, "Expect alias name after 'as'");
         char* alias_name = copy_token_text(p->previous);
         AstNode* assign = ast_new(AST_ASSIGN, p->current);
         assign->data.assign.target = alias_name;
         assign->data.assign.value = expr;
         return assign;
    }
    return expr;
}

AstNode* parse_program(Parser* p) {
    AstNode* node = ast_new(AST_PROGRAM, p->current);
    int capacity = 32;
    int count = 0;
    AstNode** stats = nr_malloc(sizeof(AstNode*) * capacity);
    while (!check(p, TOKEN_EOF)) {
        if (match(p, TOKEN_NEWLINE)) continue;
        if (check(p, TOKEN_DEDENT)) break;
        if (count >= capacity) {
            int old_cap = capacity;
            capacity *= 2;
            stats = nr_realloc(stats, sizeof(AstNode*) * old_cap, sizeof(AstNode*) * capacity);
        }
        AstNode* stmt = parse_statement(p);
        if (stmt) stats[count++] = stmt;
        else if (!check(p, TOKEN_DEDENT) && !check(p, TOKEN_EOF)) advance(p); 
    }
    node->data.program.statements = stats;
    node->data.program.count = count;
    return node;
}
