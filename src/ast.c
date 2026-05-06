#include "../include/ast.h"
#include <stdlib.h>
#include <string.h>

AstNode* ast_new(AstNodeType type, Token t) {
    AstNode* node = (AstNode*)malloc(sizeof(AstNode));
    node->type = type;
    node->line = t.line;
    node->column = t.column;
    memset(&node->data, 0, sizeof(node->data));
    return node;
}

// ast_free would be complex recursive, skipping for MVP brevity or implementing later
void ast_free(AstNode* node) {
    if (!node) return;
    // ...
    free(node);
}
