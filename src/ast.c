#include "../include/ast.h"
#include "../include/arena.h"
#include <string.h>

AstNode* ast_new(AstNodeType type, Token t) {
    AstNode* node = (AstNode*)nr_malloc(sizeof(AstNode));
    node->type = type;
    node->line = t.line;
    node->column = t.column;
    memset(&node->data, 0, sizeof(node->data));
    if (type == AST_VAR_REF || type == AST_ASSIGN) node->data.var_ref.slot = -1;
    if (type == AST_CALL) node->data.call.obj_slot = -1;
    return node;
}

void ast_free(AstNode* node) {
    // Arena handles this
}
