#include "../include/ast.h"
#include "../include/arena.h"
#include <string.h>

AstNode* ast_new(AstNodeType type, Token t) {
    AstNode* node = (AstNode*)nr_malloc(sizeof(AstNode));
    node->type = type;
    node->line = t.line;
    node->column = t.column;
    memset(&node->data, 0, sizeof(node->data));
    return node;
}

void ast_free(AstNode* node) {
    // Arena handles this
}
