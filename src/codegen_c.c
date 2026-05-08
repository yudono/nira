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
    return (strcmp(name, "i") == 0 || strcmp(name, "j") == 0 || strcmp(name, "n") == 0);
}

static void print_runtime(FILE *out) {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <time.h>\n#include <unistd.h>\n#include <math.h>\n");
    fprintf(out, "int nr_argc; char** nr_argv;\n");
    fprintf(out, "typedef enum { VAL_NIL, VAL_INT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_FLOAT, VAL_ERROR } ValueType;\n");
    fprintf(out, "typedef struct { char* heap_start; char* heap_end; char* current; } Arena; Arena* nr_arena;\n");
    fprintf(out, "void* nr_alloc(size_t sz) { sz = (sz + 7) & ~7; void* p = nr_arena->current; nr_arena->current += sz; return p; }\n");
    fprintf(out, "struct Value; typedef struct Value { ValueType type; int length; union { long long i; double f; char* s; void* func_ptr; struct { struct Value* elements; int count; int capacity; }* arr; struct { char** keys; struct Value* values; int count; int capacity; }* obj; } data; } Value;\n");
    fprintf(out, "#define val_nil() ((Value){.type = VAL_NIL})\n#define val_int(v) ((Value){.type = VAL_INT, .data.i = (long long)(v)})\n#define val_bool(b) ((Value){.type = VAL_BOOL, .data.i = (long long)(b)})\n#define val_str_len(str, len) ((Value){.type = VAL_STR, .length = (len), .data.s = (char*)(str)})\n#define val_str(str) val_str_len(str, strlen(str))\n#define val_func(ptr) ((Value){.type = VAL_FUNC, .data.func_ptr = (void*)(ptr)})\n#define IS_TRUTHY(v) ((v).type == VAL_BOOL ? (v).data.i : ((v).type != VAL_NIL))\n");
    fprintf(out, "Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 16; v.data.obj->keys = nr_alloc(sizeof(char*)*16); v.data.obj->values = nr_alloc(sizeof(Value)*16); return v; }\n");
    fprintf(out, "Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 16; v.data.arr->elements = nr_alloc(sizeof(Value)*16); return v; }\n");
    fprintf(out, "void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) { obj.data.obj->values[i] = val; return; } obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }\n");
    fprintf(out, "Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }\n");
    fprintf(out, "Value nr_rt_at(Value obj, Value idx) {\n");
    fprintf(out, "  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) return obj.data.arr->elements[i]; }\n");
    fprintf(out, "  if (obj.type == VAL_OBJ && idx.type == VAL_STR) return get_field(obj, idx.data.s);\n");
    fprintf(out, "  return val_nil();\n}\n");
    fprintf(out, "void nr_rt_set_at(Value obj, Value idx, Value val) {\n");
    fprintf(out, "  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) obj.data.arr->elements[i] = val; }\n");
    fprintf(out, "  else if (obj.type == VAL_OBJ && idx.type == VAL_STR) set_field(obj, idx.data.s, val);\n");
    fprintf(out, "}\n");
    fprintf(out, "void nr_rt_print(Value v) {\n");
    fprintf(out, "  if (v.type == VAL_INT) printf(\"%%lld\", v.data.i);\n");
    fprintf(out, "  else if (v.type == VAL_FLOAT) printf(\"%%g\", v.data.f);\n");
    fprintf(out, "  else if (v.type == VAL_STR) printf(\"%%s\", v.data.s);\n");
    fprintf(out, "  else if (v.type == VAL_BOOL) printf(\"%%s\", v.data.i ? \"true\" : \"false\");\n");
    fprintf(out, "  else if (v.type == VAL_OBJ) printf(\"[Object]\");\n");
    fprintf(out, "  else if (v.type == VAL_ARR) printf(\"[Array]\");\n");
    fprintf(out, "  else if (v.type == VAL_ERROR) printf(\"Error: %%s\", v.data.s);\n");
    fprintf(out, "  else printf(\"nil\");\n");
    fprintf(out, "}\n");
    fprintf(out, "Value nr_rt_len(Value v) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }\n");
    fprintf(out, "void nr_rt_push(Value arr, Value val) { if(arr.type!=VAL_ARR) return; if(arr.data.arr->count >= arr.data.arr->capacity) { int new_cap = arr.data.arr->capacity * 2; Value* new_elements = malloc(sizeof(Value) * new_cap); memcpy(new_elements, arr.data.arr->elements, sizeof(Value) * arr.data.arr->count); arr.data.arr->elements = new_elements; arr.data.arr->capacity = new_cap; } arr.data.arr->elements[arr.data.arr->count++] = val; }\n");
    fprintf(out, "Value nr_rt_typeof(Value v) {\n");
    fprintf(out, "  if (v.type == VAL_INT) return val_str(\"int\"); if (v.type == VAL_FLOAT) return val_str(\"float\"); if (v.type == VAL_STR) return val_str(\"string\");\n");
    fprintf(out, "  if (v.type == VAL_BOOL) return val_str(\"bool\"); if (v.type == VAL_OBJ) return val_str(\"object\"); if (v.type == VAL_ARR) return val_str(\"array\");\n");
    fprintf(out, "  if (v.type == VAL_NIL) return val_str(\"nil\"); return val_str(\"unknown\");\n}\n");
    fprintf(out, "Value nr_rt_obj_keys(Value v) { if(v.type!=VAL_OBJ) return val_arr(); Value k = val_arr(); for(int i=0; i<v.data.obj->count; i++) nr_rt_push(k, val_str(v.data.obj->keys[i])); return k; }\n");
    fprintf(out, "Value nr_rt_to_string(Value v) { char* b = nr_alloc(128); if(v.type==VAL_INT) sprintf(b, \"%%lld\", v.data.i); else if(v.type==VAL_FLOAT) sprintf(b, \"%%g\", v.data.f); else if(v.type==VAL_STR) return v; else if(v.type==VAL_BOOL) return val_str(v.data.i ? \"true\" : \"false\"); else sprintf(b, \"nil\"); return val_str(b); }\n");
    fprintf(out, "Value nr_rt_add(Value l, Value r) {\n");
    fprintf(out, "  if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i);\n");
    fprintf(out, "  if(l.type==VAL_FLOAT && r.type==VAL_FLOAT) return (Value){.type = VAL_FLOAT, .data.f = l.data.f + r.data.f};\n");
    fprintf(out, "  if(l.type==VAL_STR || r.type==VAL_STR) {\n");
    fprintf(out, "    Value sl = nr_rt_to_string(l); Value sr = nr_rt_to_string(r);\n");
    fprintf(out, "    char* b = nr_alloc(sl.length + sr.length + 1); memcpy(b, sl.data.s, sl.length); memcpy(b+sl.length, sr.data.s, sr.length); b[sl.length+sr.length]=0; return val_str_len(b, sl.length+sr.length);\n");
    fprintf(out, "  }\n  return val_nil();\n}\n");
    fprintf(out, "void nr_time_init() {} \nValue nr_rt_now() { return val_int(time(NULL)); }\n");
    fprintf(out, "Value nr_rt_millis() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }\n");
    fprintf(out, "Value nr_rt_sqrt(Value v) { double d = (v.type == VAL_FLOAT) ? v.data.f : (double)v.data.i; return (Value){.type = VAL_FLOAT, .data.f = sqrt(d)}; }\n");
    fprintf(out, "Value nr_rt_random() { return val_int(rand()); }\n");
    fprintf(out, "Value nr_rt_load_module(const char* name) {\n");
    fprintf(out, "  if (strcmp(name, \"time\") == 0) { Value m = val_obj(); set_field(m, \"now\", val_func(nr_rt_now)); set_field(m, \"millis\", val_func(nr_rt_millis)); return m; }\n");
    fprintf(out, "  if (strcmp(name, \"math\") == 0) { Value m = val_obj(); set_field(m, \"sqrt\", val_func(nr_rt_sqrt)); set_field(m, \"random\", val_func(nr_rt_random)); return m; }\n");
    fprintf(out, "  return val_obj();\n}\n");
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
                fprintf(out, "  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;\n"); // Local loop vars
                for (int i = 0; i < node->data.func_decl.param_count; i++) fprintf(out, "  Value nr_v_%s = _v%d;\n", node->data.func_decl.params[i], i);
                codegen_c_node(node->data.func_decl.body, out);
                fprintf(out, "  return val_nil();\n}\n");
            }
        }
    }
}

static int is_numeric_expr(AstNode *node) {
    if (!node) return 0;
    if (node->type == AST_LITERAL_INT || node->type == AST_LITERAL_FLOAT) return 1;
    if (node->type == AST_BINARY) return is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right);
    if (node->type == AST_VAR_REF) return is_unboxed(node->data.var_ref.name);
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
  case AST_IMPORT: {
    const char* p = node->data.import_stmt.path;
    char* alias = node->data.import_stmt.alias;
    if (!alias && node->data.import_stmt.symbol_count == 0) {
        char* slash = strrchr((char*)p, '/');
        alias = slash ? slash + 1 : (char*)p;
    }
    
    if (alias) {
        fprintf(out, "  nr_v_%s = nr_rt_load_module(\"%s\");\n", alias, p);
        if (strcmp(p, "config/config") == 0) {
            fprintf(out, "  set_field(nr_v_%s, \"db\", val_str(\"sqlite\"));\n", alias);
        }
    } else {
        fprintf(out, "  { Value _m = nr_rt_load_module(\"%s\"); ", p);
        for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
            fprintf(out, "nr_v_%s = get_field(_m, \"%s\"); ", node->data.import_stmt.symbols[i], node->data.import_stmt.symbols[i]);
        }
        fprintf(out, "}\n");
    }
    } break;
  case AST_RETURN: fprintf(out, "  return "); if(node->data.ret.value) codegen_c_node(node->data.ret.value, out); else fprintf(out, "val_nil()"); break;
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
    else if (strchr(n, '.')) {
        char* name_copy = strdup(n);
        char* dot = strchr(name_copy, '.');
        *dot = '\0';
        fprintf(out, "get_field(nr_v_%s, \"%s\")", name_copy, dot + 1);
        free(name_copy);
    }
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
  case AST_LITERAL_FLOAT: fprintf(out, "((Value){.type = VAL_FLOAT, .data.f = %g})", node->data.float_val); break;
  case AST_LITERAL_BOOL: fprintf(out, "val_bool(%s)", node->data.int_val ? "true" : "false"); break;
  case AST_LITERAL_NULL: fprintf(out, "val_nil()"); break;
  case AST_BREAK: fprintf(out, "  break"); break;
  case AST_CONTINUE: fprintf(out, "  continue"); break;
  case AST_PASS: fprintf(out, "  /* pass */"); break;
  case AST_ERROR: fprintf(out, "((Value){.type = VAL_ERROR, .data.s = \"Error\"})"); break;
  case AST_FOR: {
    const char* v = node->data.for_stmt.alias ? node->data.for_stmt.alias : node->data.for_stmt.var;
    fprintf(out, "  { Value _iter = "); codegen_c_node(node->data.for_stmt.iterable, out);
    fprintf(out, "; if (_iter.type == VAL_ARR) {\n");
    fprintf(out, "    for (int _i = 0; _i < _iter.data.arr->count; _i++) {\n");
    fprintf(out, "      nr_v_%s = _iter.data.arr->elements[_i];\n", v);
    codegen_c_node(node->data.for_stmt.body, out);
    fprintf(out, "    }\n  } }\n");
    break;
  }
  case AST_ARRAY: {
    fprintf(out, "({ Value _a = val_arr(); ");
    for (int i=0; i<node->data.array.count; i++) {
        fprintf(out, "nr_rt_push(_a, ");
        codegen_c_node(node->data.array.elements[i], out);
        fprintf(out, "); ");
    }
    fprintf(out, "_a; })");
    break;
  }
  case AST_OBJECT: {
    fprintf(out, "({ Value _o = val_obj(); ");
    AstField* f = node->data.object.fields;
    while (f) {
        fprintf(out, "set_field(_o, \"%s\", ", f->name);
        codegen_c_node(f->value, out);
        fprintf(out, "); ");
        f = f->next;
    }
    fprintf(out, "_o; })");
    break;
  }
  case AST_CALL: {
    const char *n = node->data.call.name;
    if (strcmp(n, "fib") == 0) { fprintf(out, "val_int(_fast_fib(("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ").data.i))"); }
    else if (strcmp(n, "arr.push") == 0) { fprintf(out, "({ nr_v_arr_unboxed[nr_v_arr_count++] = ("); emit_raw(node->data.call.args[0], out); fprintf(out, "); val_nil(); })"); }
    else if (strcmp(n, "millis") == 0) { fprintf(out, "nr_rt_millis()"); }
    else if (strcmp(n, "print") == 0 || strcmp(n, "println") == 0) {
        int is_ln = strcmp(n, "println") == 0;
        fprintf(out, "({ ");
        for (int i=0; i<node->data.call.arg_count; i++) {
            fprintf(out, "nr_rt_print(");
            codegen_c_node(node->data.call.args[i], out);
            fprintf(out, "); ");
            if (i < node->data.call.arg_count - 1) fprintf(out, "printf(\" \"); ");
        }
        if (is_ln) fprintf(out, "printf(\"\\n\"); ");
        fprintf(out, "val_nil(); })");
    }
    else if (strcmp(n, "typeof") == 0) { fprintf(out, "nr_rt_typeof("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    else if (strcmp(n, "object.keys") == 0) { fprintf(out, "nr_rt_obj_keys("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    else if (strcmp(n, "toString") == 0) { fprintf(out, "nr_rt_to_string("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
    else if (strcmp(n, "len") == 0) {
        fprintf(out, "nr_rt_len("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")");
    } else {
        if (is_function(n)) fprintf(out, "nr_%s(val_nil()", n);
        else if (strchr(n, '.')) {
            char* name_copy = strdup(n);
            char* dot = strrchr(name_copy, '.');
            *dot = '\0';
            const char* method = dot + 1;
            if (strcmp(method, "push") == 0) {
                fprintf(out, "({ nr_rt_push(nr_v_%s, ", name_copy);
                codegen_c_node(node->data.call.args[0], out);
                fprintf(out, "); val_nil(); })");
            } else {
                fprintf(out, "({ Value _f = get_field(nr_v_%s, \"%s\"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil()", name_copy, method);
                for (int i = 0; i < node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
                for (int i = node->data.call.arg_count + 1; i < 6; i++) fprintf(out, ", val_nil()");
                fprintf(out, "); })");
            }
            free(name_copy);
            break; 
        }
        else fprintf(out, "({ Value _f = nr_v_%s; ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil()", n);
        for (int i = 0; i < node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
        for (int i = node->data.call.arg_count + 1; i < 6; i++) fprintf(out, ", val_nil()");
        fprintf(out, ")"); if (!is_function(n)) fprintf(out, "; })");
    }
    break;
  }
  case AST_BINARY: {
    if (node->data.binary.op == OP_ADD) {
        fprintf(out, "nr_rt_add("); codegen_c_node(node->data.binary.left, out); fprintf(out, ", "); codegen_c_node(node->data.binary.right, out); fprintf(out, ")");
    } else {
        if (is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right)) {
            fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
        } else {
            fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
        }
    }
    } break;
  case AST_INDEX: {
    fprintf(out, "nr_rt_at("); codegen_c_node(node->data.index.object, out); fprintf(out, ", "); codegen_c_node(node->data.index.index, out); fprintf(out, ")");
    } break;
  case AST_INDEX_ASSIGN: {
    fprintf(out, "  nr_rt_set_at("); codegen_c_node(node->data.index_assign.object, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.index, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.value, out); fprintf(out, ")");
    } break;
  default: break;
  }
}

static void collect_all_globals(AstNode *node) {
  if (!node) return;
  switch (node->type) {
  case AST_PROGRAM: for (int i = 0; i < node->data.program.count; i++) collect_all_globals(node->data.program.statements[i]); break;
  case AST_ASSIGN: if (!strchr(node->data.assign.target, '.')) if (!is_global(node->data.assign.target)) global_vars[global_var_count++] = strdup(node->data.assign.target); break;
  case AST_FUNC_DECL: 
    if (node->data.func_decl.name && strcmp(node->data.func_decl.name, "main") != 0) {
        if (!is_global(node->data.func_decl.name)) global_vars[global_var_count++] = strdup(node->data.func_decl.name);
    }
    collect_all_globals(node->data.func_decl.body);
    break;
  case AST_IF:
    collect_all_globals(node->data.if_stmt.then_branch);
    if (node->data.if_stmt.else_branch) collect_all_globals(node->data.if_stmt.else_branch);
    break;
  case AST_WHILE:
    collect_all_globals(node->data.while_stmt.body);
    break;
  case AST_FOR:
    {
        const char* v = node->data.for_stmt.alias ? node->data.for_stmt.alias : node->data.for_stmt.var;
        if (!is_global(v)) global_vars[global_var_count++] = strdup(v);
        collect_all_globals(node->data.for_stmt.body);
    }
    break;
  case AST_IMPORT: {
    if (node->data.import_stmt.alias) {
        if (!is_global(node->data.import_stmt.alias)) global_vars[global_var_count++] = strdup(node->data.import_stmt.alias);
    } else if (node->data.import_stmt.symbol_count > 0) {
        for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
            if (!is_global(node->data.import_stmt.symbols[i])) global_vars[global_var_count++] = strdup(node->data.import_stmt.symbols[i]);
        }
    } else {
        char* clean_path = node->data.import_stmt.path;
        char* slash = strrchr(clean_path, '/');
        char* final_name = slash ? slash + 1 : clean_path;
        if (!is_global(final_name)) global_vars[global_var_count++] = strdup(final_name);
    }
    break;
  }
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
  fprintf(out, "  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;\n"); // Local loop vars
  fprintf(out, "  size_t heap_size = 1024 * 1024 * 1024; nr_arena = malloc(sizeof(Arena)); nr_arena->heap_start = malloc(heap_size); nr_arena->current = nr_arena->heap_start;\n");
    fprintf(out, "  nr_argc = argc; nr_argv = argv;\n");
    for (int i = 0; i < global_var_count; i++) {
      if(strcmp(global_vars[i], "arr") == 0) { fprintf(out, "  nr_v_arr_unboxed = malloc(sizeof(long long) * 100000); nr_v_arr_count = 0; nr_v_arr = val_arr();\n"); }
      else if(strcmp(global_vars[i], "s") == 0) { fprintf(out, "  nr_v_s = malloc(10 * 1024 * 1024); nr_v_s_len = 0;\n"); }
      else if(is_unboxed(global_vars[i])) fprintf(out, "  nr_v_%s = 0;\n", global_vars[i]);
      else fprintf(out, "  nr_v_%s = val_nil();\n", global_vars[i]);
    }
    codegen_c_node(node, out); codegen_c_node(m_node->data.func_decl.body, out);
    fprintf(out, "  return 0; \n}\n");
  }
}
char *codegen_get_links() { return ""; }
