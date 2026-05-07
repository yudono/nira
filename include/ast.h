#ifndef AST_H
#define AST_H

#include <stdlib.h>

typedef enum {
    AST_PROGRAM,
    AST_FUNC_DECL,
    AST_ASSIGN,
    AST_VAR_REF,
    AST_LITERAL_INT,
    AST_LITERAL_FLOAT,
    AST_LITERAL_STR,
    AST_OBJECT,
    AST_CALL,
    AST_RETURN,
    AST_IF,
    AST_FOR,
    AST_IMPORT,
    AST_EXPORT,
    AST_ARRAY,
    AST_INDEX,
    AST_INDEX_ASSIGN,
    AST_BINARY,
    AST_DESTRUCTURING,
    AST_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_PASS,
    AST_LITERAL_BOOL,
    AST_LITERAL_NULL,
    AST_ERROR,
    AST_NATIVE
} AstNodeType;

struct AstNode;

typedef struct AstField {
    char* name;
    struct AstNode* value;
    char* alias; // For { name as alias }
    struct AstField* next;
} AstField;

typedef struct AstNode {
    AstNodeType type;
    int line;
    int column;
    union {
        // Program
        struct {
            struct AstNode** statements;
            int count;
        } program;

        // Function Declaration
        struct {
            char* name;
            char** params;
            int param_count;
            struct AstNode* body;
            int is_exported;
            int is_unpacking;
        } func_decl;

        // Assignment
        struct {
            char* target;
            struct AstNode* value;
        } assign;

        // Variable Reference
        char* var_name;

        // Literals
        int int_val;
        double float_val;
        char* str_val;

        // Object Literal
        struct {
            AstField* fields;
        } object;

        // Call
        struct {
            char* name;
            struct AstNode** args;
            int arg_count;
        } call;

        // Binary
        struct {
            struct AstNode* left;
            char* op;
            struct AstNode* right;
        } binary;

        // If
        struct {
            struct AstNode* condition;
            struct AstNode* then_branch;
            struct AstNode* else_branch;
        } if_stmt;

        // For
        struct {
            char* var;
            char* alias; // For 'for item as alias in items'
            struct AstNode* iterable;
            struct AstNode* body;
        } for_stmt;

        // Array
        struct {
            struct AstNode** elements;
            int count;
        } array;

        // Index access
        struct {
            struct AstNode* object;
            struct AstNode* index;
        } index;

        // Index assignment
        struct {
            struct AstNode* object;
            struct AstNode* index;
            struct AstNode* value;
        } index_assign;

        // Import
        struct {
            char* path;
            char* alias; // For 'import http as net'
            char** symbols;
            int symbol_count;
            struct AstNode* module_prog; // Store the imported module's program
        } import_stmt;

        // Return
        struct {
            struct AstNode* value;
        } ret;
        // Destructuring
        struct {
            struct AstNode* target; // AST_OBJECT
            struct AstNode* value;
        } destruct;

        // While
        struct {
            struct AstNode* condition;
            struct AstNode* body;
        } while_stmt;

        // Error
        struct {
            struct AstNode* message;
        } error_expr;
        
        // Native (FFI/Linker info)
        struct {
            char** links;
            int link_count;
            char** headers;
            int header_count;
            char* code;
        } native_stmt;
    } data;
} AstNode;

#include "lexer.h"

AstNode* ast_new(AstNodeType type, Token t);
void ast_free(AstNode* node);

#endif
