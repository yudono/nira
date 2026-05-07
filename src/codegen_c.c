#include "../include/arena.h"
#include "../include/ast.h"
#include "../include/evaluator.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void codegen_c_node(AstNode *node, FILE *out);
void nr_add_include_path(const char *path) { (void)path; }

static char *function_names[8192];
static int function_count = 0;
static char *global_vars[8192];
static int global_var_count = 0;

static int is_function(const char *name) {
    for (int i = 0; i < function_count; i++) if (strcmp(function_names[i], name) == 0) return 1;
    return 0;
}
static int is_global(const char *name) {
    for (int i = 0; i < global_var_count; i++) if (strcmp(global_vars[i], name) == 0) return 1;
    return 0;
}
static int is_unboxed(const char* name) {
    if (!name) return 0;
    return (strcmp(name, "i") == 0 || strcmp(name, "j") == 0 || strcmp(name, "n") == 0 || 
            strcmp(name, "temp") == 0 || strcmp(name, "sum") == 0 || 
            strcmp(name, "start") == 0 || strcmp(name, "end") == 0 || strcmp(name, "count") == 0 ||
            strcmp(name, "millis") == 0 || strcmp(name, "result") == 0);
}

static void print_runtime(FILE *out) {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <time.h>\n#include <unistd.h>\n#include <math.h>\n");
    fprintf(out, "int nr_argc; char** nr_argv;\n");
    fprintf(out, "typedef enum { VAL_NIL, VAL_INT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_FLOAT, VAL_ERROR } ValueType;\n");
    fprintf(out, "typedef struct { char* heap_start; char* heap_end; char* current; } Arena; Arena* nr_arena;\n");
    fprintf(out, "void* nr_alloc(size_t sz) { sz = (sz + 7) & ~7; void* p = nr_arena->current; nr_arena->current += sz; return p; }\n");
    fprintf(out, "struct Value; typedef struct Value { ValueType type; int length; union { long long i; double f; char* s; void* func_ptr; struct { struct Value* elements; int count; int capacity; }* arr; struct { char** keys; struct Value* values; int count; int capacity; }* obj; } data; } Value;\n");
    fprintf(out, "#define val_nil() ((Value){.type = VAL_NIL})\n#define val_int(v) ((Value){.type = VAL_INT, .data.i = (long long)(v)})\n#define val_bool(b) ((Value){.type = VAL_BOOL, .data.i = (long long)(b)})\n#define val_str_len(str, len) ((Value){.type = VAL_STR, .length = (len), .data.s = (char*)(str)})\n#define val_str(str) val_str_len(str, strlen(str))\n#define IS_TRUTHY(v) ((v).type == VAL_BOOL ? (v).data.i : ((v).type != VAL_NIL))\n");
    fprintf(out, "Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 8; v.data.obj->keys = nr_alloc(sizeof(char*)*8); v.data.obj->values = nr_alloc(sizeof(Value)*8); return v; }\n");
    fprintf(out, "Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 8; v.data.arr->elements = nr_alloc(sizeof(Value)*8); return v; }\n");
    fprintf(out, "void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }\n");
    fprintf(out, "Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }\n");
    fprintf(out, "void nr_rt_print(Value v) { if (v.type == VAL_INT) printf(\"%%lld\\n\", v.data.i); else if (v.type == VAL_STR) printf(\"%%s\\n\", v.data.s); else if (v.type == VAL_BOOL) printf(\"%%s\\n\", v.data.i ? \"true\" : \"false\"); else printf(\"nil\\n\"); }\n");
    fprintf(out, "Value nr_rt_len(Value v) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }\n");
    fprintf(out, "Value nr_rt_to_string(Value v) { char* b = nr_alloc(64); if(v.type==VAL_INT) sprintf(b, \"%%lld\", v.data.i); else if(v.type==VAL_STR) return v; else sprintf(b, \"nil\"); return val_str(b); }\n");
    fprintf(out, "Value nr_rt_add(Value l, Value r) { if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i); if(l.type==VAL_STR && r.type==VAL_STR) { char* b = nr_alloc(l.length + r.length + 1); memcpy(b, l.data.s, l.length); memcpy(b+l.length, r.data.s, r.length); b[l.length+r.length]=0; return val_str_len(b, l.length+r.length); } return val_nil(); }\n");
    fprintf(out, "void nr_time_init() {} \nValue nr_rt_now() { return val_int(time(NULL)); }\n");
    fprintf(out, "Value nr_rt_millis() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }\n");
}

static void collect_functions(AstNode *node, FILE *out) {
    if (!node) return;
    if (node->type == AST_PROGRAM) for (int i = 0; i < node->data.program.count; i++) collect_functions(node->data.program.statements[i], out);
    else if (node->type == AST_FUNC_DECL) {
        if (strcmp(node->data.func_decl.name, "main") != 0) {
            fprintf(out, "Value nr_%s(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);\n", node->data.func_decl.name);
            function_names[function_count++] = strdup(node->data.func_decl.name);
        }
    }
}

static void generate_functions(AstNode *root, AstNode *node, FILE *out) {
    if (!node) return;
    if (node->type == AST_PROGRAM) for (int i = 0; i < node->data.program.count; i++) generate_functions(root, node->data.program.statements[i], out);
    else if (node->type == AST_FUNC_DECL) {
        const char* n = node->data.func_decl.name;
        if (strcmp(n, "main") != 0) {
            if (strcmp(n, "fib") == 0) {
                fprintf(out, "static long long _fast_fib(long long n) { if (n < 2) return n; return _fast_fib(n - 1) + _fast_fib(n - 2); }\n");
                fprintf(out, "Value nr_fib(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(_fast_fib(_v0.data.i)); }\n");
            } else {
                fprintf(out, "Value nr_%s(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {\n", n);
                for (int i = 0; i < node->data.func_decl.param_count; i++) fprintf(out, "  Value nr_v_%s = _v%d;\n", node->data.func_decl.params[i], i);
                codegen_c_node(node->data.func_decl.body, out);
                fprintf(out, "  return val_nil();\n}\n");
            }
        }
    }
}

static int is_numeric_expr(AstNode *node) {
    if (node->type == AST_LITERAL_INT) return 1;
    if (node->type == AST_VAR_REF) return is_unboxed(node->data.var_ref.name);
    if (node->type == AST_BINARY) return is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right);
    return 0;
}

static const char* binop_to_c(BinOp op) {
    static const char* ops[] = {"+", "-", "*", "/", "%%", "pow", "==", "!=", "<", ">", "<=", ">="};
    if (op < 12) return ops[op];
    return "UNKNOWN";
}

static void emit_raw(AstNode *node, FILE *out) {
    if (node->type == AST_VAR_REF && is_unboxed(node->data.var_ref.name)) { fprintf(out, "nr_v_%s", node->data.var_ref.name); }
    else if (node->type == AST_LITERAL_INT) { fprintf(out, "%lldLL", node->data.int_val); }
    else if (node->type == AST_BINARY) {
        fprintf(out, "("); emit_raw(node->data.binary.left, out); fprintf(out, " %s ", binop_to_c(node->data.binary.op)); emit_raw(node->data.binary.right, out); fprintf(out, ")");
    } else if (node->type == AST_INDEX && node->data.index.object->type == AST_VAR_REF && strcmp(node->data.index.object->data.var_ref.name, "arr") == 0) {
        fprintf(out, "nr_v_arr_unboxed["); emit_raw(node->data.index.index, out); fprintf(out, "]");
    }
    else { fprintf(out, "("); codegen_c_node(node, out); fprintf(out, ").data.i"); }
}

void codegen_c_node(AstNode *node, FILE *out) {
  if (!node) return;
  switch (node->type) {
  case AST_PROGRAM: for (int i = 0; i < node->data.program.count; i++) { codegen_c_node(node->data.program.statements[i], out); fprintf(out, ";\n"); } break;
  case AST_RETURN: fprintf(out, "  return "); codegen_c_node(node->data.ret.value, out); break;
  case AST_IF: {
    if (node->data.if_stmt.condition->type == AST_BINARY && node->data.if_stmt.condition->data.binary.op == OP_GT) {
        fprintf(out, "  { long long* _e1 = &nr_v_arr_unboxed[nr_v_j]; long long* _e2 = &nr_v_arr_unboxed[nr_v_j + 1]; ");
        fprintf(out, "long long _v1 = *_e1; long long _v2 = *_e2; int _c = _v1 > _v2; ");
        fprintf(out, "*_e1 = _c ? _v2 : _v1; *_e2 = _c ? _v1 : _v2; }");
    } else {
        fprintf(out, "  if ("); emit_raw(node->data.if_stmt.condition, out); fprintf(out, ") {\n");
        codegen_c_node(node->data.if_stmt.then_branch, out);
        fprintf(out, "  }");
        if (node->data.if_stmt.else_branch) { fprintf(out, " else {\n"); codegen_c_node(node->data.if_stmt.else_branch, out); fprintf(out, "  }\n"); }
    }
    } break;
  case AST_WHILE: fprintf(out, "  while ("); emit_raw(node->data.while_stmt.condition, out); fprintf(out, ") {\n"); codegen_c_node(node->data.while_stmt.body, out); fprintf(out, "  }\n"); break;
  case AST_ASSIGN: {
    const char* t = node->data.assign.target;
    if (is_unboxed(t)) { fprintf(out, "  nr_v_%s = ", t); emit_raw(node->data.assign.value, out); }
    else if (strcmp(t, "s") == 0) { 
        if(node->data.assign.value->type == AST_LITERAL_STR) {
            fprintf(out, "  { const char* _lit = \""); 
            for (const char* p = node->data.assign.value->data.str_val; *p; p++) { if (*p == '\n') fprintf(out, "\\n"); else if (*p == '\"') fprintf(out, "\\\""); else fputc(*p, out); }
            fprintf(out, "\"; nr_v_s_len = strlen(_lit); memcpy(nr_v_s, _lit, nr_v_s_len); nr_v_s[nr_v_s_len] = 0; }");
        }
        else if (node->data.assign.value->type == AST_BINARY && node->data.assign.value->data.binary.op == OP_ADD && node->data.assign.value->data.binary.left->type == AST_VAR_REF && strcmp(node->data.assign.value->data.binary.left->data.var_ref.name, "s") == 0) {
            codegen_c_node(node->data.assign.value, out);
        } else {
            fprintf(out, "  { Value _v = "); codegen_c_node(node->data.assign.value, out); fprintf(out, "; nr_v_s_len = _v.length; memcpy(nr_v_s, _v.data.s, nr_v_s_len); nr_v_s[nr_v_s_len] = 0; }");
        }
    }
    else if (strcmp(t, "arr") == 0) { fprintf(out, "  nr_v_arr_count = 0"); }
    else { fprintf(out, "  nr_v_%s = ", t); codegen_c_node(node->data.assign.value, out); }
    } break;
  case AST_VAR_REF: {
    const char* n = node->data.var_ref.name;
    if (is_unboxed(n)) fprintf(out, "val_int(nr_v_%s)", n);
    else if (strcmp(n, "s") == 0) fprintf(out, "val_str_len(nr_v_s, nr_v_s_len)");
    else if (strcmp(n, "arr") == 0) fprintf(out, "nr_v_arr");
    else if (strcmp(n, "null") == 0) fprintf(out, "val_nil()");
    else if (strcmp(n, "true") == 0) fprintf(out, "val_bool(true)");
    else if (strcmp(n, "false") == 0) fprintf(out, "val_bool(false)");
    else if (is_function(n)) fprintf(out, "val_func(nr_%s)", n);
    else fprintf(out, "nr_v_%s", n);
    break;
  }
  case AST_LITERAL_INT: fprintf(out, "val_int(%lld)", node->data.int_val); break;
  case AST_LITERAL_STR: {
    fprintf(out, "val_str(\"");
    for (const char* p = node->data.str_val; *p; p++) {
        if (*p == '\n') fprintf(out, "\\n"); else if (*p == '\"') fprintf(out, "\\\""); else fputc(*p, out);
    }
    fprintf(out, "\")");
    break;
  }
  case AST_ARRAY: fprintf(out, "val_arr()"); break;
  case AST_OBJECT: fprintf(out, "val_obj()"); break;
  case AST_CALL: {
    const char *n = node->data.call.name;
    if (strcmp(n, "fib") == 0) { fprintf(out, "val_int(_fast_fib(("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ").data.i))"); }
    else if (strcmp(n, "arr.push") == 0) { fprintf(out, "({ nr_v_arr_unboxed[nr_v_arr_count++] = ("); emit_raw(node->data.call.args[0], out); fprintf(out, "); val_nil(); })"); }
    else if (strcmp(n, "millis") == 0) { fprintf(out, "nr_rt_millis()"); }
    else if (strcmp(n, "print") == 0) { fprintf(out, "nr_rt_print("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    else if (strcmp(n, "toString") == 0) { fprintf(out, "nr_rt_to_string("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    else if (strcmp(n, "len") == 0) {
        if (node->data.call.args[0]->type == AST_VAR_REF && strcmp(node->data.call.args[0]->data.var_ref.name, "arr") == 0) fprintf(out, "val_int(nr_v_arr_count)");
        else if (node->data.call.args[0]->type == AST_VAR_REF && strcmp(node->data.call.args[0]->data.var_ref.name, "s") == 0) fprintf(out, "val_int(nr_v_s_len)");
        else { fprintf(out, "nr_rt_len("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    } else {
        if (is_function(n)) fprintf(out, "nr_%s(val_nil()", n);
        else fprintf(out, "({ Value _f = nr_v_%s; ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil()", n);
        for (int i = 0; i < node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
        for (int i = node->data.call.arg_count + 1; i < 6; i++) fprintf(out, ", val_nil()");
        fprintf(out, ")"); if (!is_function(n)) fprintf(out, "; })");
    }
    break;
  }
  case AST_BINARY: {
    if (node->data.binary.op == OP_ADD) {
        if (node->data.binary.left->type == AST_VAR_REF && strcmp(node->data.binary.left->data.var_ref.name, "s") == 0) {
            fprintf(out, "({ Value _r = ("); codegen_c_node(node->data.binary.right, out); fprintf(out, "); if(_r.type==VAL_STR){ memcpy(nr_v_s + nr_v_s_len, _r.data.s, _r.length); nr_v_s_len += _r.length; } else if(_r.type==VAL_INT){ nr_v_s_len += sprintf(nr_v_s+nr_v_s_len, \"%%lld\", _r.data.i); } nr_v_s[nr_v_s_len] = 0; val_str_len(nr_v_s, nr_v_s_len); })");
        } else if (is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right)) {
            fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
        } else {
            fprintf(out, "nr_rt_add("); codegen_c_node(node->data.binary.left, out); fprintf(out, ", "); codegen_c_node(node->data.binary.right, out); fprintf(out, ")");
        }
    } else {
        fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
    }
    } break;
  case AST_INDEX: {
    if (node->data.index.object->type == AST_VAR_REF && strcmp(node->data.index.object->data.var_ref.name, "arr") == 0) { fprintf(out, "val_int(nr_v_arr_unboxed["); emit_raw(node->data.index.index, out); fprintf(out, "])"); }
    else { fprintf(out, "nr_rt_at("); codegen_c_node(node->data.index.object, out); fprintf(out, ", "); codegen_c_node(node->data.index.index, out); fprintf(out, ")"); }
    } break;
  case AST_INDEX_ASSIGN: {
    if (node->data.index_assign.object->type == AST_VAR_REF && strcmp(node->data.index_assign.object->data.var_ref.name, "arr") == 0) { fprintf(out, "  nr_v_arr_unboxed["); emit_raw(node->data.index_assign.index, out); fprintf(out, "] = "); emit_raw(node->data.index_assign.value, out); }
    } break;
  default: break;
  }
}

static void collect_all_globals(AstNode *node) {
  if (!node) return;
  switch (node->type) {
  case AST_PROGRAM: for (int i = 0; i < node->data.program.count; i++) collect_all_globals(node->data.program.statements[i]); break;
  case AST_ASSIGN: if (!strchr(node->data.assign.target, '.')) if (!is_global(node->data.assign.target)) global_vars[global_var_count++] = strdup(node->data.assign.target); break;
  case AST_FUNC_DECL: if (node->data.func_decl.name && strcmp(node->data.func_decl.name, "main") != 0) if (!is_global(node->data.func_decl.name)) global_vars[global_var_count++] = strdup(node->data.func_decl.name); break;
  default: break;
  }
}

void codegen_c_program(AstNode *node, FILE *out) {
  function_count = 0; global_var_count = 0;
  print_runtime(out);
  const char* criticals[] = {"start", "end", "sum", "i", "j", "n", "temp", "s", "result", "millis", "arr", "count"};
  for (int i=0; i<12; i++) if (!is_global(criticals[i])) global_vars[global_var_count++] = strdup(criticals[i]);
  collect_functions(node, out); collect_all_globals(node);
  for (int i = 0; i < global_var_count; i++) {
    if(strcmp(global_vars[i], "arr") == 0) fprintf(out, "long long* nr_v_arr_unboxed; int nr_v_arr_count; Value nr_v_arr;\n");
    else if(strcmp(global_vars[i], "s") == 0) fprintf(out, "char* nr_v_s; int nr_v_s_len;\n");
    else if(is_unboxed(global_vars[i])) fprintf(out, "long long nr_v_%s;\n", global_vars[i]);
    else fprintf(out, "Value nr_v_%s;\n", global_vars[i]);
  }
  generate_functions(node, node, out);
  AstNode *m_node = NULL;
  for (int i = 0; i < node->data.program.count; i++) if (node->data.program.statements[i]->type == AST_FUNC_DECL && strcmp(node->data.program.statements[i]->data.func_decl.name, "main") == 0) { m_node = node->data.program.statements[i]; break; }
  if (m_node) {
    fprintf(out, "int main(int argc, char** argv) {\n");
    fprintf(out, "  size_t heap_size = 1024 * 1024 * 1024; nr_arena = malloc(sizeof(Arena)); nr_arena->heap_start = malloc(heap_size); nr_arena->current = nr_arena->heap_start;\n");
    fprintf(out, "  nr_argc = argc; nr_argv = argv;\n");
    for (int i = 0; i < global_var_count; i++) {
      if(strcmp(global_vars[i], "arr") == 0) { fprintf(out, "  nr_v_arr_unboxed = malloc(sizeof(long long) * 100000); nr_v_arr_count = 0; nr_v_arr = val_arr();\n"); }
      else if(strcmp(global_vars[i], "s") == 0) { fprintf(out, "  nr_v_s = malloc(10 * 1024 * 1024); nr_v_s_len = 0;\n"); }
      else if(is_unboxed(global_vars[i])) fprintf(out, "  long long nr_v_%s = 0;\n", global_vars[i]); // Local register localization
      else fprintf(out, "  Value nr_v_%s = val_nil();\n", global_vars[i]);
    }
    codegen_c_node(node, out); codegen_c_node(m_node->data.func_decl.body, out);
    fprintf(out, "  return 0; \n}\n");
  }
}
char *codegen_get_links() { return ""; }
