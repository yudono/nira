#include "../include/evaluator.h"

#define EVAL_FAST(n, e)                                                        \
  ((n)->type == AST_LITERAL_INT                                                \
       ? (Value){.type = VAL_INT, .length = 0, .data.i = (n)->data.int_val}    \
       : ((n)->type == AST_VAR_REF && (n)->data.var_ref.slot >= 0 &&           \
                  (e)->slots                                                   \
              ? (e)->slots[(n)->data.var_ref.slot]                             \
              : eval(n, e)))

#include "../include/arena.h"
#include "../include/parser.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>

extern char nira_std_lib_path[1024];

extern int g_argc;
extern char **g_argv;

static char *nr_read_file_internal(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  rewind(file);
  char *buf = nr_malloc(size + 1);
  fread(buf, 1, size, file);
  buf[size] = '\0';
  fclose(file);
  return buf;
}

static Value val_to_str(Value v);
static char *nr_json_encode(Value v) {
  if (v.type == VAL_INT) {
    char b[64];
    sprintf(b, "%lld", v.data.i);
    return nr_strdup(b);
  }
  if (v.type == VAL_FLOAT) {
    char b[64];
    sprintf(b, "%g", v.data.f);
    return nr_strdup(b);
  }
  if (v.type == VAL_BOOL)
    return nr_strdup(v.data.i ? "true" : "false");
  if (v.type == VAL_NIL)
    return nr_strdup("null");
  if (v.type == VAL_STR) {
    char *b = nr_malloc(strlen(v.data.s) * 2 + 3);
    sprintf(b, "\"%s\"", v.data.s); // Simple escape-less for now
    return b;
  }
  if (v.type == VAL_ARR) {
    char *b = nr_strdup("[");
    for (int i = 0; i < v.data.arr->count; i++) {
      char *item = nr_json_encode(*v.data.arr->elements[i]);
      char *next = nr_malloc(strlen(b) + strlen(item) + 3);
      sprintf(next, "%s%s%s", b, item, (i == v.data.arr->count - 1) ? "" : ",");
      b = next;
    }
    char *final = nr_malloc(strlen(b) + 2);
    sprintf(final, "%s]", b);
    return final;
  }
  if (v.type == VAL_OBJ) {
    char *b = nr_strdup("{");
    for (int i = 0; i < v.data.obj->count; i++) {
      char *val = nr_json_encode(*v.data.obj->values[i]);
      char *next =
          nr_malloc(strlen(b) + strlen(v.data.obj->keys[i]) + strlen(val) + 6);
      sprintf(next, "%s\"%s\":%s%s", b, v.data.obj->keys[i], val,
              (i == v.data.obj->count - 1) ? "" : ",");
      b = next;
    }
    char *final = nr_malloc(strlen(b) + 2);
    sprintf(final, "%s}", b);
    return final;
  }
  return nr_strdup("null");
}

static Value nr_json_parse(const char **p) {
  while (isspace(**p))
    (*p)++;
  if (**p == '{') {
    (*p)++;
    Value obj = val_obj();
    while (**p && **p != '}') {
      while (isspace(**p))
        (*p)++;
      if (**p == '"')
        (*p)++;
      const char *start = *p;
      while (**p && **p != '"')
        (*p)++;
      char *key = nr_malloc(*p - start + 1);
      strncpy(key, start, *p - start);
      key[*p - start] = '\0';
      if (**p == '"')
        (*p)++;
      while (isspace(**p))
        (*p)++;
      if (**p == ':')
        (*p)++;
      Value val = nr_json_parse(p);
      set_field(obj, key, val);
      while (isspace(**p))
        (*p)++;
      if (**p == ',')
        (*p)++;
    }
    if (**p == '}')
      (*p)++;
    return obj;
  }
  if (**p == '[') {
    (*p)++;
    Value arr = val_arr();
    while (**p && **p != ']') {
      Value val = nr_json_parse(p);
      nr_rt_push(arr, val);
      while (isspace(**p))
        (*p)++;
      if (**p == ',')
        (*p)++;
    }
    if (**p == ']')
      (*p)++;
    return arr;
  }
  if (**p == '"') {
    (*p)++;
    const char *start = *p;
    while (**p && **p != '"')
      (*p)++;
    char *s = nr_malloc(*p - start + 1);
    strncpy(s, start, *p - start);
    s[*p - start] = '\0';
    if (**p == '"')
      (*p)++;
    return val_str(s);
  }
  if (isdigit(**p) || **p == '-') {
    char *end;
    long long i = strtoll(*p, &end, 10);
    *p = end;
    return val_int(i);
  }
  if (strncmp(*p, "true", 4) == 0) {
    *p += 4;
    return val_bool(1);
  }
  if (strncmp(*p, "false", 5) == 0) {
    *p += 5;
    return val_bool(0);
  }
  if (strncmp(*p, "null", 4) == 0) {
    *p += 4;
    return val_nil();
  }
  return val_nil();
}

void *dl_handles[64];
int dl_handle_count = 0;

typedef struct ResolverScope {
  struct ResolverScope *parent;
  char *vars[256];
  int count;
  int max_count;
} ResolverScope;

static void nr_resolve_node(AstNode *node, ResolverScope *scope) {
  if (!node)
    return;
  switch (node->type) {
  case AST_PROGRAM:
    for (int i = 0; i < node->data.program.count; i++)
      nr_resolve_node(node->data.program.statements[i], scope);
    break;
  case AST_FUNC_DECL: {
    if (node->data.func_decl.param_defaults) {
      for (int i = 0; i < node->data.func_decl.param_count; i++) {
        if (node->data.func_decl.param_defaults[i]) {
          nr_resolve_node(node->data.func_decl.param_defaults[i], scope);
        }
      }
    }
    ResolverScope inner = {.parent = scope, .count = 0, .max_count = 0};
    for (int i = 0; i < node->data.func_decl.param_count; i++) {
      inner.vars[inner.count++] = node->data.func_decl.params[i];
    }
    if (inner.count > inner.max_count) inner.max_count = inner.count;
    nr_resolve_node(node->data.func_decl.body, &inner);
    node->data.func_decl.local_count = inner.max_count;
    break;
  }
  case AST_VAR_REF: {
    node->data.var_ref.slot = -1;
    ResolverScope *s = scope;
    while (s) {
      for (int i = 0; i < s->count; i++) {
        if (strcmp(node->data.var_ref.name, s->vars[i]) == 0) {
          if (s == scope)
            node->data.var_ref.slot = i;
          break;
        }
      }
      if (node->data.var_ref.slot != -1)
        break;
      s = s->parent;
    }
    break;
  }
  case AST_ASSIGN: {
    node->data.assign.slot = -1;
    int found = -1;
    
    // Check all scopes to see if it already exists
    ResolverScope *s = scope;
    int is_local = 1;
    while (s) {
      for (int i = 0; i < s->count; i++) {
        if (strcmp(node->data.assign.target, s->vars[i]) == 0) {
          found = i;
          break;
        }
      }
      if (found != -1) {
        if (is_local) node->data.assign.slot = found;
        break;
      }
      s = s->parent;
      is_local = 0;
    }
    
    // If not found anywhere, define it locally
    if (found == -1 && scope->count < 256) {
      node->data.assign.slot = scope->count;
      scope->vars[scope->count++] = node->data.assign.target;
      if (scope->count > scope->max_count) scope->max_count = scope->count;
    }
    
    nr_resolve_node(node->data.assign.value, scope);
    break;
  }
  case AST_BINARY:
    nr_resolve_node(node->data.binary.left, scope);
    nr_resolve_node(node->data.binary.right, scope);
    break;
  case AST_CALL: {
    if (node->data.call.name) {
      char *dot = strchr(node->data.call.name, '.');
      if (dot) {
        int len = dot - node->data.call.name;
        char obj_name[64];
        if (len < 63) {
          strncpy(obj_name, node->data.call.name, len);
          obj_name[len] = '\0';

          // Resolve obj_name to slot
          ResolverScope *s = scope;
          while (s) {
            for (int i = 0; i < s->count; i++) {
              if (strcmp(obj_name, s->vars[i]) == 0) {
                if (s == scope)
                  node->data.call.obj_slot = i;
                break;
              }
            }
            if (node->data.call.obj_slot != -1)
              break;
            s = s->parent;
          }
        }
      }
    }
    for (int i = 0; i < node->data.call.arg_count; i++)
      nr_resolve_node(node->data.call.args[i], scope);
    break;
  }
  case AST_IF:
    nr_resolve_node(node->data.if_stmt.condition, scope);
    nr_resolve_node(node->data.if_stmt.then_branch, scope);
    nr_resolve_node(node->data.if_stmt.else_branch, scope);
    break;
  case AST_RETURN:
    nr_resolve_node(node->data.ret.value, scope);
    break;
  case AST_FOR: {
    int old_count = scope->count;
    if (scope->count < 256) {
      node->data.for_stmt.slot = scope->count;
      scope->vars[scope->count++] = node->data.for_stmt.alias
                                         ? node->data.for_stmt.alias
                                         : node->data.for_stmt.var;
      if (scope->count > scope->max_count) scope->max_count = scope->count;
    } else {
      node->data.for_stmt.slot = -1;
    }
    nr_resolve_node(node->data.for_stmt.iterable, scope);
    nr_resolve_node(node->data.for_stmt.body, scope);
    scope->count = old_count;
    break;
  }
  case AST_WHILE:
    nr_resolve_node(node->data.while_stmt.condition, scope);
    nr_resolve_node(node->data.while_stmt.body, scope);
    break;
  case AST_INDEX:
    nr_resolve_node(node->data.index.object, scope);
    nr_resolve_node(node->data.index.index, scope);
    break;
  case AST_INDEX_ASSIGN:
    nr_resolve_node(node->data.index_assign.object, scope);
    nr_resolve_node(node->data.index_assign.index, scope);
    nr_resolve_node(node->data.index_assign.value, scope);
    break;
  case AST_ARRAY:
    for (int i = 0; i < node->data.array.count; i++)
      nr_resolve_node(node->data.array.elements[i], scope);
    break;
  case AST_OBJECT: {
    AstField *f = node->data.object.fields;
    while (f) {
      nr_resolve_node(f->value, scope);
      f = f->next;
    }
    break;
  }
  case AST_DESTRUCTURING:
    nr_resolve_node(node->data.destruct.value, scope);
    if (node->data.destruct.target)
      nr_resolve_node(node->data.destruct.target, scope);
    break;
  default:
    break;
  }
}

void nr_resolve(AstNode *program) {
  ResolverScope global = {.parent = NULL, .count = 0};
  nr_resolve_node(program, &global);
}

static void array_push(Array *arr, Value val) {
  if (arr->count >= arr->capacity) {
    int old_cap = arr->capacity;
    arr->capacity *= 2;
    arr->elements = nr_realloc(arr->elements, sizeof(Value *) * old_cap,
                               sizeof(Value *) * arr->capacity);
  }
  arr->elements[arr->count] = nr_malloc(sizeof(Value));
  *arr->elements[arr->count] = val;
  arr->count++;
}

void eval_set_field(Object *obj, const char *key, Value val) {
  for (int i = 0; i < obj->count; i++) {
    if (strcmp(obj->keys[i], key) == 0) {
      *obj->values[i] = val;
      return;
    }
  }
  if (obj->count >= obj->capacity) {
    int old_cap = obj->capacity;
    obj->capacity *= 2;
    obj->keys = nr_realloc(obj->keys, sizeof(char *) * old_cap,
                           sizeof(char *) * obj->capacity);
    obj->values = nr_realloc(obj->values, sizeof(Value *) * old_cap,
                             sizeof(Value *) * obj->capacity);
  }
  obj->keys[obj->count] = nr_strdup(key);
  obj->values[obj->count] = nr_malloc(sizeof(Value));
  *obj->values[obj->count] = val;
  obj->count++;
}

// --- Include Paths ---
static char *include_paths[16];
static int include_path_count = 0;
extern char nira_std_lib_path[1024];
extern char nira_global_libs_path[1024];

void nr_eval_add_include_path(const char *path) {
  if (include_path_count < 16) {
    include_paths[include_path_count++] = nr_strdup(path);
  }
}

// --- Value Constructors ---

Value val_int(long long i) { return (Value){.type = VAL_INT, .data.i = i}; }
Value val_float(double f) { return (Value){.type = VAL_FLOAT, .data.f = f}; }
Value val_str(char *s) {
  if (!s)
    s = "";
  int len = strlen(s);
  return (Value){.type = VAL_STR, .length = len, .data.s = nr_strdup(s)};
}
Value val_str_len(char *s, int len) {
  return (Value){.type = VAL_STR, .length = len, .data.s = s};
}
Value val_nil() { return (Value){.type = VAL_NIL}; }
Value val_bool(int b) { return (Value){.type = VAL_BOOL, .data.i = b}; }
Value val_error(char *msg) {
  return (Value){.type = VAL_ERROR,
                 .data.s = msg ? nr_strdup(msg) : nr_strdup("error")};
}

Value val_return(Value v) {
  Value res;
  res.type = VAL_RETURN;
  res.data.return_val = nr_malloc(sizeof(Value));
  *res.data.return_val = v;
  return res;
}

Value val_obj() {
  Value v;
  v.type = VAL_OBJ;
  v.data.obj = nr_malloc(sizeof(Object));
  v.data.obj->count = 0;
  v.data.obj->capacity = 16;
  v.data.obj->keys = nr_malloc(sizeof(char *) * 16);
  v.data.obj->values = nr_malloc(sizeof(Value *) * 16);
  return v;
}

Value val_arr() {
  Value v;
  v.type = VAL_ARR;
  v.data.arr = nr_malloc(sizeof(Array));
  v.data.arr->count = 0;
  v.data.arr->capacity = 16;
  v.data.arr->elements = nr_malloc(sizeof(Value *) * 16);
  return v;
}

Value val_func(AstNode *decl, Environment *closure) {
  return (Value){
      .type = VAL_FUNC, .data.func.decl = decl, .data.func.closure = closure};
}

// --- Environment ---

static unsigned int hash_key(const char *key) {
  unsigned int hash = 5381;
  int c;
  while ((c = *key++))
    hash = ((hash << 5) + hash) + c;
  return hash;
}

#define ENV_POOL_SIZE 1024

#define VALUE_STACK_MAX 10000000
static Value value_stack[VALUE_STACK_MAX];
static int value_stack_ptr = 0;

static Environment *env_pool[ENV_POOL_SIZE];
static int env_pool_count = 0;

Environment *env_new(Environment *parent, int slot_count) {
  Environment *env = nr_malloc(sizeof(Environment));
  env->parent = parent;
  env->slot_count = slot_count;
  env->count = 0;
  env->capacity = 0;
  env->table = NULL;
  env->slots = NULL;
  env->source = NULL;
  env->filename = NULL;

  if (slot_count > 0) {
    env->slots = &value_stack[value_stack_ptr];
    value_stack_ptr += slot_count;
    for (int i = 0; i < slot_count; i++)
      env->slots[i].type = VAL_NIL;
  }
  return env;
}

void env_free(Environment *env) {
  // Do nothing for now to prevent closure corruption
  // The arena will clean up everything at the end of the request/program
}

static void env_resize(Environment *env) {
  int old_cap = env->capacity;
  if (old_cap == 0) {
    env->capacity = 4;
    env->table = env->fast_table;
    for (int i = 0; i < 4; i++)
      env->table[i] = NULL;
    return;
  }
  Variable **old_table = env->table;
  env->capacity *= 2;
  env->table = nr_malloc(sizeof(Variable *) * env->capacity);
  for (int i = 0; i < env->capacity; i++)
    env->table[i] = NULL;

  for (int i = 0; i < old_cap; i++) {
    Variable *v = old_table[i];
    while (v) {
      Variable *next = v->next;
      unsigned int h = hash_key(v->name) % env->capacity;
      v->next = env->table[h];
      env->table[h] = v;
      v = next;
    }
  }
}

void env_define(Environment *env, char *name, Value val) {
  if (!name)
    return;
  if (env->capacity == 0 || env->count >= env->capacity * 0.75)
    env_resize(env);

  unsigned int h = hash_key(name) % env->capacity;
  Variable *v = env->table[h];
  while (v) {
    if (strcmp(v->name, name) == 0) {
      v->value = val;
      return;
    }
    v = v->next;
  }

  v = nr_malloc(sizeof(Variable));
  v->name = nr_strdup(name);
  v->value = val;
  v->next = env->table[h];
  env->table[h] = v;
  env->count++;
}

void env_assign(Environment *env, char *name, Value val) {
  if (!name)
    return;
  Environment *curr = env;
  while (curr) {
    if (curr->capacity > 0) {
      unsigned int h = hash_key(name) % curr->capacity;
      Variable *v = curr->table[h];
      while (v) {
        if (strcmp(v->name, name) == 0) {
          v->value = val;
          return;
        }
        v = v->next;
      }
    }
    curr = curr->parent;
  }
  env_define(env, name, val);
}

Value env_get_by_hash(Environment *env, char *name, unsigned int h) {
  if (!name)
    return val_nil();
  Environment *curr = env;
  int depth = 0;
  while (curr) {
    if (depth > 100) {
      fprintf(stderr, "ERROR: Parent environment loop detected for '%s'\n", name);
      return val_nil();
    }
    if (curr->capacity > 0) {
      unsigned int idx = h % curr->capacity;
      Variable *v = curr->table[idx];
      while (v) {
        if (v->name && strcmp(v->name, name) == 0) {
          return v->value;
        }
        v = v->next;
      }
    }
    curr = curr->parent;
    depth++;
  }
  return val_nil();
}

Value env_get(Environment *env, char *name) {
  if (!name)
    return val_nil();
  return env_get_by_hash(env, name, hash_key(name));
}

// --- FFI Compatibility ---

void *nr_alloc(size_t sz) { return nr_malloc(sz); }

Value nr_rt_push(Value arr, Value val) {
  if (arr.type != VAL_ARR)
    return val_nil();
  if (arr.data.arr->count >= arr.data.arr->capacity) {
    int old_cap = arr.data.arr->capacity;
    arr.data.arr->capacity *= 2;
    Value **new_el = nr_alloc(sizeof(Value *) * arr.data.arr->capacity);
    memcpy(new_el, arr.data.arr->elements, sizeof(Value *) * old_cap);
    arr.data.arr->elements = new_el;
  }
  arr.data.arr->elements[arr.data.arr->count] = nr_alloc(sizeof(Value));
  *arr.data.arr->elements[arr.data.arr->count] = val;
  arr.data.arr->count++;
  return val;
}

void set_field(Value obj, const char *key, Value val) {
  if (obj.type != VAL_OBJ)
    return;
  for (int i = 0; i < obj.data.obj->count; i++) {
    if (strcmp(obj.data.obj->keys[i], key) == 0) {
      *obj.data.obj->values[i] = val;
      return;
    }
  }
  if (obj.data.obj->count >= obj.data.obj->capacity) {
    int old_cap = obj.data.obj->capacity;
    obj.data.obj->capacity *= 2;
    char **new_keys = nr_alloc(sizeof(char *) * obj.data.obj->capacity);
    Value **new_vals = nr_alloc(sizeof(Value *) * obj.data.obj->capacity);
    memcpy(new_keys, obj.data.obj->keys, sizeof(char *) * old_cap);
    memcpy(new_vals, obj.data.obj->values, sizeof(Value *) * old_cap);
    obj.data.obj->keys = new_keys;
    obj.data.obj->values = new_vals;
  }
  obj.data.obj->keys[obj.data.obj->count] = nr_strdup(key);
  obj.data.obj->values[obj.data.obj->count] = nr_alloc(sizeof(Value));
  *obj.data.obj->values[obj.data.obj->count] = val;
  obj.data.obj->count++;
}

Value get_field(Value obj, const char *key) {
  if (obj.type != VAL_OBJ)
    return val_nil();
  for (int i = 0; i < obj.data.obj->count; i++) {
    if (strcmp(obj.data.obj->keys[i], key) == 0)
      return *obj.data.obj->values[i];
  }
  return val_nil();
}

// --- Diagnostics ---

static void report_runtime_error(AstNode *node, Environment *env,
                                 const char *name, const char *msg) {
  const char *source = NULL;
  const char *filename = "source.nr";
  Environment *curr = env;
  while (curr) {
    if (curr->filename && strcmp(filename, "source.nr") == 0) {
      filename = curr->filename;
    }
    if (curr->source && !source) {
      source = curr->source;
    }
    if (strcmp(filename, "source.nr") != 0 && source) {
      break;
    }
    curr = curr->parent;
  }

  fprintf(stderr, "\n\033[1;31m[%s ERROR]\033[0m %s\n", name, msg);
  fprintf(stderr, "\033[1;34m-->\033[0m %s:%d:%d\n", filename, node->line, node->column);

  if (source) {
    int line = 1;
    int i = 0;
    while (line < node->line && source[i] != '\0') {
      if (source[i] == '\n')
        line++;
      i++;
    }
    const char *start = source + i;
    const char *end = start;
    while (*end != '\n' && *end != '\0')
      end++;

    fprintf(stderr, " \033[1;34m|\033[0m\n");
    fprintf(stderr, " \033[1;34m| \033[0m %.*s\n", (int)(end - start), start);
    fprintf(stderr, " \033[1;34m| \033[1;31m");
    for (int j = 1; j < node->column; j++)
      fprintf(stderr, " ");
    fprintf(stderr, "^\033[0m\n");
    fprintf(stderr, " \033[1;34m|\033[0m\n\n");
  }
  exit(1);
}

// --- Helpers ---
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const unsigned char *data, size_t input_length) {
  size_t output_length = 4 * ((input_length + 2) / 3);
  char *encoded_data = nr_malloc(output_length + 1);
  for (size_t i = 0, j = 0; j < output_length;) {
    uint32_t octet_a = i < input_length ? data[i++] : 0;
    uint32_t octet_b = i < input_length ? data[i++] : 0;
    uint32_t octet_c = i < input_length ? data[i++] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
    encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
    encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
    if (j < output_length) {
      encoded_data[j++] =
          (i > input_length + 1) ? '=' : base64_table[(triple >> 6) & 0x3F];
    }
    if (j < output_length) {
      encoded_data[j++] =
          (i > input_length) ? '=' : base64_table[triple & 0x3F];
    }
  }
  encoded_data[output_length] = '\0';
  return encoded_data;
}

unsigned char *base64_decode(const char *data, size_t input_length,
                             size_t *output_length) {
  if (input_length % 4 != 0)
    return NULL;
  *output_length = input_length / 4 * 3;
  if (data[input_length - 1] == '=')
    (*output_length)--;
  if (data[input_length - 2] == '=')
    (*output_length)--;
  unsigned char *decoded_data = nr_malloc(*output_length + 1);
  static char decoding_table[256];
  static int table_built = 0;
  if (!table_built) {
    for (int i = 0; i < 64; i++)
      decoding_table[(unsigned char)base64_table[i]] = i;
    table_built = 1;
  }
  for (size_t i = 0, j = 0; i < input_length;) {
    uint32_t sextet_a =
        data[i] == '=' ? 0 : decoding_table[(unsigned char)data[i++]];
    uint32_t sextet_b =
        data[i] == '=' ? 0 : decoding_table[(unsigned char)data[i++]];
    uint32_t sextet_c =
        data[i] == '=' ? 0 : decoding_table[(unsigned char)data[i++]];
    uint32_t sextet_d =
        data[i] == '=' ? 0 : decoding_table[(unsigned char)data[i++]];
    uint32_t triple =
        (sextet_a << 18) + (sextet_b << 12) + (sextet_c << 6) + sextet_d;
    if (j < *output_length)
      decoded_data[j++] = (triple >> 16) & 0xFF;
    if (j < *output_length)
      decoded_data[j++] = (triple >> 8) & 0xFF;
    if (j < *output_length)
      decoded_data[j++] = triple & 0xFF;
  }
  decoded_data[*output_length] = '\0';
  return decoded_data;
}

static char *read_file_internal(const char *path) {
  FILE *file = fopen(path, "rb");
  if (file == NULL)
    return NULL;
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  char *buffer = (char *)nr_malloc(fileSize + 1);
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

static char *val_to_json_internal(Value v) {
  if (v.type == VAL_INT) {
    char *b = nr_malloc(32);
    sprintf(b, "%lld", v.data.i);
    return b;
  }
  if (v.type == VAL_STR) {
    char *b = nr_malloc(strlen(v.data.s) + 3);
    sprintf(b, "\"%s\"", v.data.s);
    return b;
  }
  if (v.type == VAL_BOOL)
    return nr_strdup(v.data.i ? "true" : "false");
  if (v.type == VAL_ARR) {
    char *res = nr_strdup("[");
    for (int i = 0; i < v.data.arr->count; i++) {
      char *item = val_to_json_internal(*v.data.arr->elements[i]);
      char *old = res;
      res = nr_malloc(strlen(old) + strlen(item) + 3);
      sprintf(res, "%s%s%s", old, item, i < v.data.arr->count - 1 ? "," : "");
    }
    char *old = res;
    res = nr_malloc(strlen(old) + 2);
    sprintf(res, "%s]", old);
    return res;
  }
  if (v.type == VAL_OBJ) {
    char *res = nr_strdup("{");
    for (int i = 0; i < v.data.obj->count; i++) {
      char *val = val_to_json_internal(*v.data.obj->values[i]);
      char *old = res;
      res = nr_malloc(strlen(old) + strlen(v.data.obj->keys[i]) + strlen(val) +
                      6);
      sprintf(res, "%s\"%s\":%s%s", old, v.data.obj->keys[i], val,
              i < v.data.obj->count - 1 ? "," : "");
    }
    char *old = res;
    res = nr_malloc(strlen(old) + 2);
    sprintf(res, "%s}", old);
    return res;
  }
  return nr_strdup("null");
}

static Value get_dot_value(Value obj, char *field) {
  if (obj.type == VAL_ARR) {
    if (strcmp(field, "length") == 0)
      return val_int(obj.data.arr->count);
  }
  if (obj.type == VAL_STR) {
    if (strcmp(field, "length") == 0)
      return val_int(strlen(obj.data.s));
  }
  if (obj.type != VAL_OBJ || !field)
    return val_nil();

  char *first_dot = strchr(field, '.');
  if (first_dot) {
    char base[64];
    int len = first_dot - field;
    if (len >= 63)
      return val_nil();
    strncpy(base, field, len);
    base[len] = '\0';

    Value sub_obj = val_nil();
    for (int i = 0; i < obj.data.obj->count; i++) {
      if (obj.data.obj->keys[i] && strcmp(obj.data.obj->keys[i], base) == 0) {
        sub_obj = *obj.data.obj->values[i];
        break;
      }
    }
    return get_dot_value(sub_obj, first_dot + 1);
  }

  for (int i = 0; i < obj.data.obj->count; i++) {
    if (obj.data.obj->keys[i] && strcmp(obj.data.obj->keys[i], field) == 0) {
      Value res = *obj.data.obj->values[i];
      return res;
    }
  }
  return val_nil();
}

static int is_truthy(Value v) {
  if (v.type == VAL_NIL)
    return 0;
  if (v.type == VAL_BOOL)
    return v.data.i;
  if (v.type == VAL_INT)
    return v.data.i != 0;
  return 1;
}

static int is_safe_path(const char *path) {
  if (!path)
    return 0;
  // Basic check for traversal
  if (strstr(path, ".."))
    return 0;
  return 1;
}

void print_value(Value v) {
  if (v.type == VAL_INT)
    printf("%lld", v.data.i);
  else if (v.type == VAL_FLOAT)
    printf("%g", v.data.f);
  else if (v.type == VAL_BOOL)
    printf("%s", v.data.i ? "true" : "false");
  else if (v.type == VAL_STR)
    printf("%s", v.data.s);
  else if (v.type == VAL_OBJ)
    printf("[Object]");
  else if (v.type == VAL_ARR)
    printf("[Array]");
  else if (v.type == VAL_NIL)
    printf("nil");
}

// --- Evaluator ---

static Value eval_binary(AstNode *node, Environment *env) {
  Value left;
  AstNode *l_node = node->data.binary.left;
  // printf("[DEBUG] eval_binary: op=%d\n", node->data.binary.op);
  if (l_node->type == AST_VAR_REF && l_node->data.var_ref.slot != -1 &&
      env->slots) {
    left = env->slots[l_node->data.var_ref.slot];
  } else if (l_node->type == AST_LITERAL_INT) {
    left = (Value){.type = VAL_INT, .data.i = l_node->data.int_val};
  } else {
    left = EVAL_FAST(l_node, env);
    if (left.type == VAL_RETURN || left.type == VAL_BREAK ||
        left.type == VAL_CONTINUE)
      return left;
  }

  BinOp op = node->data.binary.op;

  if (op == OP_AND) {
    if (!is_truthy(left))
      return left;
    return eval(node->data.binary.right, env);
  }
  if (op == OP_OR) {
    if (is_truthy(left))
      return left;
    return eval(node->data.binary.right, env);
  }
  if (op == OP_NOT) {
    return val_bool(!is_truthy(left));
  }

  Value right;
  AstNode *r_node = node->data.binary.right;
  if (r_node) {
    if (r_node->type == AST_VAR_REF && r_node->data.var_ref.slot != -1 &&
        env->slots) {
      right = env->slots[r_node->data.var_ref.slot];
    } else if (r_node->type == AST_LITERAL_INT) {
      right = (Value){.type = VAL_INT, .data.i = r_node->data.int_val};
    } else {
      right = EVAL_FAST(r_node, env);
      if (right.type == VAL_RETURN || right.type == VAL_BREAK ||
          right.type == VAL_CONTINUE)
        return right;
    }
  } else {
    right = val_nil();
  }

  // Optimized Integer Path (Hot Loop)
  if (left.type == VAL_INT && right.type == VAL_INT) {
    int l = left.data.i;
    int r = right.data.i;
    switch (op) {
    case OP_ADD:
      return (Value){.type = VAL_INT, .data.i = l + r};
    case OP_SUB:
      return (Value){.type = VAL_INT, .data.i = l - r};
    case OP_MUL:
      return (Value){.type = VAL_INT, .data.i = l * r};
    case OP_LT:
      return (Value){.type = VAL_BOOL, .data.i = l < r};
    case OP_GT:
      return (Value){.type = VAL_BOOL, .data.i = l > r};
    case OP_EQ:
      return (Value){.type = VAL_BOOL, .data.i = l == r};
    case OP_NEQ:
      return (Value){.type = VAL_BOOL, .data.i = l != r};
    case OP_LE:
      return (Value){.type = VAL_BOOL, .data.i = l <= r};
    case OP_GE:
      return (Value){.type = VAL_BOOL, .data.i = l >= r};
    case OP_DIV:
      if (r == 0)
        report_runtime_error(node, env, "MATH", "Division by zero");
      return (Value){.type = VAL_INT, .data.i = l / r};
    default:
      break;
    }
  }

  // Generic Path
  if ((left.type == VAL_INT || left.type == VAL_FLOAT) &&
      (right.type == VAL_INT || right.type == VAL_FLOAT)) {
    double l = (left.type == VAL_FLOAT) ? left.data.f : (double)left.data.i;
    double r = (right.type == VAL_FLOAT) ? right.data.f : (double)right.data.i;
    int is_f = (left.type == VAL_FLOAT || right.type == VAL_FLOAT);

    switch (op) {
    case OP_ADD:
      return is_f ? val_float(l + r) : val_int((int)(l + r));
    case OP_SUB:
      return is_f ? val_float(l - r) : val_int((int)(l - r));
    case OP_MUL:
      return is_f ? val_float(l * r) : val_int((int)(l * r));
    case OP_DIV:
      if (r == 0)
        report_runtime_error(node, env, "MATH", "Division by zero");
      return val_float(l / r);
    case OP_LT:
      return val_bool(l < r);
    case OP_GT:
      return val_bool(l > r);
    case OP_EQ:
      return val_bool(l == r);
    default:
      break;
    }
  }

  if (op == OP_EQ) {
    if (left.type != right.type)
      return val_bool(0);
    switch (left.type) {
    case VAL_NIL:
      return val_bool(1);
    case VAL_INT:
      return val_bool(left.data.i == right.data.i);
    case VAL_FLOAT:
      return val_bool(left.data.f == right.data.f);
    case VAL_STR:
      return val_bool(strcmp(left.data.s, right.data.s) == 0);
    case VAL_BOOL:
      return val_bool(left.data.i == right.data.i);
    case VAL_OBJ:
      return val_bool(left.data.obj == right.data.obj);
    case VAL_ARR:
      return val_bool(left.data.arr == right.data.arr);
    default:
      return val_bool(0);
    }
  }
  if (op == OP_NEQ) {
    if (left.type != right.type)
      return val_bool(1);
    switch (left.type) {
    case VAL_NIL:
      return val_bool(0);
    case VAL_INT:
      return val_bool(left.data.i != right.data.i);
    case VAL_FLOAT:
      return val_bool(left.data.f != right.data.f);
    case VAL_STR:
      return val_bool(strcmp(left.data.s, right.data.s) != 0);
    case VAL_BOOL:
      return val_bool(left.data.i != right.data.i);
    case VAL_OBJ:
      return val_bool(left.data.obj != right.data.obj);
    case VAL_ARR:
      return val_bool(left.data.arr != right.data.arr);
    default:
      return val_bool(1);
    }
  }

  if (left.type == VAL_STR && op == OP_ADD) {
    Value sr = val_to_str(right);
    char *res = nr_malloc(strlen(left.data.s) + strlen(sr.data.s) + 1);
    strcpy(res, left.data.s);
    strcat(res, sr.data.s);
    return val_str(res);
  }
  if (right.type == VAL_STR && op == OP_ADD) {
    Value sl = val_to_str(left);
    char *res = nr_malloc(strlen(sl.data.s) + strlen(right.data.s) + 1);
    strcpy(res, sl.data.s);
    strcat(res, right.data.s);
    return val_str(res);
  }

  return val_nil();
}

static Value val_to_str(Value v) {
  if (v.type == VAL_RETURN)
    v = *v.data.return_val;
  char buf[128];
  if (v.type == VAL_INT) {
    snprintf(buf, sizeof(buf), "%lld", v.data.i);
    return val_str(nr_strdup(buf));
  }
  if (v.type == VAL_FLOAT) {
    snprintf(buf, sizeof(buf), "%g", v.data.f);
    return val_str(nr_strdup(buf));
  }
  if (v.type == VAL_BOOL)
    return val_str(nr_strdup(v.data.i ? "true" : "false"));
  if (v.type == VAL_STR)
    return v;
  if (v.type == VAL_NIL)
    return val_str(nr_strdup("nil"));
  if (v.type == VAL_OBJ)
    return val_str(nr_strdup("[Object]"));
  if (v.type == VAL_ARR)
    return val_str(nr_strdup("[Array]"));
  return val_str(nr_strdup("[Value]"));
}

static Value call_native_func(void *ptr, AstNode *node, Environment *env) {
  int argc = node->data.call.arg_count;
  Value args[10];
  for (int i = 0; i < argc && i < 10; i++) {
    args[i] = eval(node->data.call.args[i], env);
    if (args[i].type == VAL_RETURN)
      args[i] = *args[i].data.return_val;
  }

  switch (argc) {
  case 0:
    return ((Value(*)())ptr)();
  case 1:
    return ((Value(*)(Value))ptr)(args[0]);
  case 2:
    return ((Value(*)(Value, Value))ptr)(args[0], args[1]);
  case 3:
    return ((Value(*)(Value, Value, Value))ptr)(args[0], args[1], args[2]);
  case 4:
    return ((Value(*)(Value, Value, Value, Value))ptr)(args[0], args[1], args[2],
                                                       args[3]);
  case 5:
    return ((Value(*)(Value, Value, Value, Value, Value))ptr)(
        args[0], args[1], args[2], args[3], args[4]);
  case 6:
    return ((Value(*)(Value, Value, Value, Value, Value, Value))ptr)(
        args[0], args[1], args[2], args[3], args[4], args[5]);
  default:
    return val_nil();
  }
}

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *nr_base64_encode(const char *src, int len) {
  int out_len = 4 * ((len + 2) / 3);
  char *res = nr_malloc(out_len + 1);
  int i, j;
  for (i = 0, j = 0; i < len;) {
    uint32_t octet_a = i < len ? (unsigned char)src[i++] : 0;
    uint32_t octet_b = i < len ? (unsigned char)src[i++] : 0;
    uint32_t octet_c = i < len ? (unsigned char)src[i++] : 0;
    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
    res[j++] = b64_table[(triple >> 3 * 6) & 0x3F];
    res[j++] = b64_table[(triple >> 2 * 6) & 0x3F];
    res[j++] = b64_table[(triple >> 1 * 6) & 0x3F];
    res[j++] = b64_table[(triple >> 0 * 6) & 0x3F];
  }
  for (int k = 0; k < (3 - len % 3) % 3; k++) res[out_len - 1 - k] = '=';
  res[out_len] = '\0';
  return res;
}

static char *nr_base64_decode(const char *src) {
  int len = strlen(src);
  if (len % 4 != 0) return nr_strdup("");
  int out_len = len / 4 * 3;
  if (src[len - 1] == '=') out_len--;
  if (src[len - 2] == '=') out_len--;
  char *res = nr_malloc(out_len + 1);
  static char dec_table[256];
  static int init = 0;
  if (!init) {
    for (int i = 0; i < 64; i++) dec_table[(int)b64_table[i]] = i;
    init = 1;
  }
  for (int i = 0, j = 0; i < len;) {
    uint32_t sextet_a = src[i] == '=' ? 0 & i++ : dec_table[(int)src[i++]];
    uint32_t sextet_b = src[i] == '=' ? 0 & i++ : dec_table[(int)src[i++]];
    uint32_t sextet_c = src[i] == '=' ? 0 & i++ : dec_table[(int)src[i++]];
    uint32_t sextet_d = src[i] == '=' ? 0 & i++ : dec_table[(int)src[i++]];
    uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
    if (j < out_len) res[j++] = (triple >> 2 * 8) & 0xFF;
    if (j < out_len) res[j++] = (triple >> 1 * 8) & 0xFF;
    if (j < out_len) res[j++] = (triple >> 0 * 8) & 0xFF;
  }
  res[out_len] = '\0';
  return res;
}

static char *nr_bin2hex(const char *src, int len) {
  char *res = nr_malloc(len * 2 + 1);
  for (int i = 0; i < len; i++) {
    sprintf(res + (i * 2), "%02x", (unsigned char)src[i]);
  }
  res[len * 2] = '\0';
  return res;
}

static char *nr_hex2bin(const char *src) {
  int len = strlen(src);
  if (len % 2 != 0) return nr_strdup("");
  char *res = nr_malloc(len / 2 + 1);
  for (int i = 0; i < len / 2; i++) {
    unsigned int val;
    sscanf(src + (i * 2), "%2x", &val);
    res[i] = (char)val;
  }
  res[len / 2] = '\0';
  return res;
}

static Value eval_call(AstNode *node, Environment *env) {
  Value func_val = val_nil();
  Value obj = val_nil();
  int is_sugar = 0;

  if (node->type == AST_CALL) {
    if (!node->data.call.name) return val_nil();
    char *full_name = nr_strdup(node->data.call.name);
    char *dot = strchr(full_name, '.');

    if (dot) {
      *dot = '\0';
      char *obj_name = full_name;
      char *field_name = dot + 1;

      if (node->data.call.obj_slot != -1 && env->slots) {
        obj = env->slots[node->data.call.obj_slot];
      } else {
        obj = env_get(env, obj_name);
        if (obj.type == VAL_NIL && strcmp(obj_name, "object") == 0) {
          obj = val_obj();
        }
      }

      // Handle Built-in Methods
      if (obj.type == VAL_ARR) {
        if (strcmp(field_name, "push") == 0) {
          if (node->data.call.arg_count > 0) {
            Value val = eval(node->data.call.args[0], env);
            if (val.type == VAL_RETURN) val = *val.data.return_val;
            nr_rt_push(obj, val);
            return val;
          }
        } else if (strcmp(field_name, "pop") == 0) {
          Array *arr = obj.data.arr;
          if (arr->count > 0) {
            arr->count--;
            return *arr->elements[arr->count];
          }
          return val_nil();
        } else if (strcmp(field_name, "length") == 0) {
          return val_int(obj.data.arr->count);
        }
      }

      if (obj.type == VAL_STR && strcmp(field_name, "length") == 0) {
        return val_int(strlen(obj.data.s));
      }

      if (strcmp(obj_name, "object") == 0 && strcmp(field_name, "keys") == 0) {
        if (node->data.call.arg_count == 1) {
          Value arg = eval(node->data.call.args[0], env);
          if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
          if (arg.type == VAL_OBJ) {
            Value arr = val_arr();
            for (int i = 0; i < arg.data.obj->count; i++) {
              if (arg.data.obj->keys[i]) {
                nr_rt_push(arr, val_str(arg.data.obj->keys[i]));
              }
            }
            return arr;
          }
        }
        return val_arr();
      }

      // Resolve field function
      func_val = get_dot_value(obj, field_name);

      // Method Sugar Fallback
      if (func_val.type == VAL_NIL) {
        func_val = env_get(env, field_name);
        if (func_val.type == VAL_FUNC) {
          is_sugar = 1;
        }
      }
    } else {
      if (strcmp(full_name, "typeof") == 0) {
        if (node->data.call.arg_count == 1) {
          Value v = eval(node->data.call.args[0], env);
          if (v.type == VAL_RETURN) v = *v.data.return_val;
          switch (v.type) {
            case VAL_NIL: return val_str(nr_strdup("nil"));
            case VAL_INT: return val_str(nr_strdup("int"));
            case VAL_FLOAT: return val_str(nr_strdup("float"));
            case VAL_STR: return val_str(nr_strdup("string"));
            case VAL_OBJ: return val_str(nr_strdup("object"));
            case VAL_ARR: return val_str(nr_strdup("array"));
            case VAL_FUNC: return val_str(nr_strdup("function"));
            case VAL_BOOL: return val_str(nr_strdup("bool"));
            default: return val_str(nr_strdup("unknown"));
          }
        }
        return val_str(nr_strdup("unknown"));
      }

      if (strcmp(full_name, "__builtin_keys") == 0) {
        if (node->data.call.arg_count == 1) {
          Value arg = eval(node->data.call.args[0], env);
          if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
          if (arg.type == VAL_OBJ) {
            Value arr = val_arr();
            for (int i = 0; i < arg.data.obj->count; i++) {
              if (arg.data.obj->keys[i]) {
                nr_rt_push(arr, val_str(nr_strdup(arg.data.obj->keys[i])));
              }
            }
            return arr;
          }
        }
        return val_arr();
      }

      // Global Built-ins
      if (strcmp(full_name, "__builtin_encoding_to_base64") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          if (data.type == VAL_STR) return val_str(nr_base64_encode(data.data.s, strlen(data.data.s)));
        }
        return val_nil();
      }
      if (strcmp(full_name, "__builtin_encoding_from_base64") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          if (data.type == VAL_STR) return val_str(nr_base64_decode(data.data.s));
        }
        return val_nil();
      }
      if (strcmp(full_name, "__builtin_encoding_to_hex") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          if (data.type == VAL_STR) return val_str(nr_bin2hex(data.data.s, strlen(data.data.s)));
        }
        return val_nil();
      }
      if (strcmp(full_name, "__builtin_encoding_from_hex") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          if (data.type == VAL_STR) return val_str(nr_hex2bin(data.data.s));
        }
        return val_nil();
      }

      if (strcmp(full_name, "__builtin_json_parse") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          if (data.type == VAL_STR) {
            const char *p = data.data.s;
            return nr_json_parse(&p);
          }
        }
        return val_nil();
      }
      if (strcmp(full_name, "__builtin_json_stringify") == 0) {
        if (node->data.call.arg_count == 1) {
          Value data = eval(node->data.call.args[0], env);
          if (data.type == VAL_RETURN) data = *data.data.return_val;
          return val_str(nr_json_encode(data));
        }
        return val_nil();
      }

      if (strcmp(full_name, "print") == 0 || strcmp(full_name, "println") == 0) {
        for (int i = 0; i < node->data.call.arg_count; i++) {
          Value v = eval(node->data.call.args[i], env);
          if (v.type == VAL_RETURN) v = *v.data.return_val;
          print_value(v);
          if (i < node->data.call.arg_count - 1) printf(" ");
        }
        printf("\n");
        fflush(stdout);
        return val_nil();
      }
      
      if (strcmp(full_name, "toString") == 0 || strcmp(full_name, "__builtin_conv_to_str") == 0) {
        Value v = eval(node->data.call.args[0], env);
        return val_to_str(v);
      }

      if (strcmp(full_name, "__builtin_conv_to_int") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_STR) return val_int(atoll(v.data.s));
        if (v.type == VAL_FLOAT) return val_int((long long)v.data.f);
        if (v.type == VAL_BOOL) return val_int(v.data.i);
        if (v.type == VAL_INT) return v;
        return val_int(0);
      }

      if (strcmp(full_name, "__builtin_conv_to_float") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_STR) return val_float(atof(v.data.s));
        if (v.type == VAL_INT) return val_float((double)v.data.i);
        if (v.type == VAL_BOOL) return val_float((double)v.data.i);
        if (v.type == VAL_FLOAT) return v;
        return val_float(0);
      }

      if (strcmp(full_name, "__builtin_conv_to_bool") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_INT) return val_bool(v.data.i != 0);
        if (v.type == VAL_FLOAT) return val_bool(v.data.f != 0);
        if (v.type == VAL_STR) return val_bool(v.data.s && strlen(v.data.s) > 0);
        if (v.type == VAL_NIL) return val_bool(0);
        return val_bool(1);
      }

      if (strcmp(full_name, "__builtin_parse_int") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_STR) return val_int(atoll(v.data.s));
        return val_int(0);
      }

      if (strcmp(full_name, "__builtin_parse_float") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_STR) return val_float(atof(v.data.s));
        return val_float(0);
      }

      if (strcmp(full_name, "__builtin_parse_bool") == 0) {
        Value v = eval(node->data.call.args[0], env);
        if (v.type == VAL_RETURN) v = *v.data.return_val;
        if (v.type == VAL_STR) {
            if (strcmp(v.data.s, "true") == 0) return val_bool(1);
            if (strcmp(v.data.s, "false") == 0) return val_bool(0);
        }
        return val_bool(0);
      }

      if (strcmp(full_name, "len") == 0) {
        if (node->data.call.arg_count == 1) {
          Value arg = eval(node->data.call.args[0], env);
          if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
          if (arg.type == VAL_ARR) return val_int(arg.data.arr->count);
          if (arg.type == VAL_STR) return val_int(strlen(arg.data.s));
        }
        return val_int(0);
      }

      if (strcmp(full_name, "__builtin_obj_keys") == 0) {
        if (node->data.call.arg_count == 1) {
          Value arg = eval(node->data.call.args[0], env);
          if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
          if (arg.type == VAL_OBJ) {
            Value arr = val_arr();
            for (int i = 0; i < arg.data.obj->count; i++) {
              if (arg.data.obj->keys[i]) {
                nr_rt_push(arr, val_str(arg.data.obj->keys[i]));
              }
            }
            return arr;
          }
        }
        return val_arr();
      }
      
      if (strcmp(full_name, "__builtin_file_read") == 0) {
        Value path = eval(node->data.call.args[0], env);
        if (path.type != VAL_STR) return val_nil();
        char *s = nr_read_file_internal(path.data.s);
        return s ? val_str(s) : val_nil();
      }

      if (strcmp(full_name, "__builtin_time_now") == 0) {
        return val_int((long long)time(NULL));
      }

      if (strcmp(full_name, "__builtin_time_to_unix") == 0) {
        if (node->data.call.arg_count == 1) {
          Value obj = eval(node->data.call.args[0], env);
          if (obj.type == VAL_RETURN) obj = *obj.data.return_val;
          if (obj.type == VAL_OBJ) {
            struct tm t = {0};
            t.tm_year = (int)get_field(obj, "year").data.i - 1900;
            t.tm_mon = (int)get_field(obj, "month").data.i - 1;
            t.tm_mday = (int)get_field(obj, "day").data.i;
            return val_int((long long)mktime(&t));
          }
        }
        return val_int(0);
      }

      if (strcmp(full_name, "__builtin_delay") == 0) {
        if (node->data.call.arg_count == 1) {
          Value ms = eval(node->data.call.args[0], env);
          if (ms.type == VAL_RETURN) ms = *ms.data.return_val;
          if (ms.type == VAL_INT) usleep(ms.data.i * 1000);
        }
        return val_nil();
      }

      if (strcmp(full_name, "__builtin_args") == 0) {
        Value arr = val_arr();
        for (int i = 0; i < g_argc; i++) {
          nr_rt_push(arr, val_str(g_argv[i]));
        }
        return arr;
      }

      if (strcmp(full_name, "__builtin_exit_proc") == 0) {
        if (node->data.call.arg_count == 1) {
          Value code = eval(node->data.call.args[0], env);
          if (code.type == VAL_RETURN) code = *code.data.return_val;
          exit((int)code.data.i);
        }
        exit(0);
        return val_nil();
      }

      if (strcmp(full_name, "__builtin_sqlite3_open") == 0) {
        Value path = eval(node->data.call.args[0], env);
        if (path.type == VAL_RETURN) path = *path.data.return_val;
        if (path.type != VAL_STR) return val_nil();
        sqlite3 *db;
        if (sqlite3_open(path.data.s, &db) != SQLITE_OK) return val_nil();
        return val_int((long long)db);
      }

      if (strcmp(full_name, "__builtin_sqlite3_close") == 0) {
        Value db_val = eval(node->data.call.args[0], env);
        if (db_val.type == VAL_RETURN) db_val = *db_val.data.return_val;
        if (db_val.type != VAL_INT) return val_nil();
        sqlite3_close((sqlite3 *)db_val.data.i);
        return val_nil();
      }

      if (strcmp(full_name, "__builtin_sqlite3_exec") == 0) {
        Value db_val = eval(node->data.call.args[0], env);
        if (db_val.type == VAL_RETURN) db_val = *db_val.data.return_val;
        Value sql = eval(node->data.call.args[1], env);
        if (sql.type == VAL_RETURN) sql = *sql.data.return_val;
        Value params = eval(node->data.call.args[2], env);
        if (params.type == VAL_RETURN) params = *params.data.return_val;

        if (db_val.type != VAL_INT || sql.type != VAL_STR) return val_nil();
        sqlite3 *db = (sqlite3 *)db_val.data.i;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL) != SQLITE_OK)
          return val_error((char *)sqlite3_errmsg(db));

        if (params.type == VAL_ARR) {
          for (int i = 0; i < params.data.arr->count; i++) {
            Value p = *params.data.arr->elements[i];
            if (p.type == VAL_INT) sqlite3_bind_int64(stmt, i + 1, p.data.i);
            else if (p.type == VAL_FLOAT) sqlite3_bind_double(stmt, i + 1, p.data.f);
            else if (p.type == VAL_STR) sqlite3_bind_text(stmt, i + 1, p.data.s, -1, SQLITE_TRANSIENT);
            else if (p.type == VAL_NIL) sqlite3_bind_null(stmt, i + 1);
          }
        }
        int res = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return val_bool(res == SQLITE_DONE || res == SQLITE_OK);
      }

      if (strcmp(full_name, "__builtin_sqlite3_query") == 0) {
        Value db_val = eval(node->data.call.args[0], env);
        if (db_val.type == VAL_RETURN) db_val = *db_val.data.return_val;
        Value sql = eval(node->data.call.args[1], env);
        if (sql.type == VAL_RETURN) sql = *sql.data.return_val;
        Value params = eval(node->data.call.args[2], env);
        if (params.type == VAL_RETURN) params = *params.data.return_val;

        if (db_val.type != VAL_INT || sql.type != VAL_STR) return val_nil();
        sqlite3 *db = (sqlite3 *)db_val.data.i;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL) != SQLITE_OK)
          return val_error((char *)sqlite3_errmsg(db));

        if (params.type == VAL_ARR) {
          for (int i = 0; i < params.data.arr->count; i++) {
            Value p = *params.data.arr->elements[i];
            if (p.type == VAL_INT) sqlite3_bind_int64(stmt, i + 1, p.data.i);
            else if (p.type == VAL_FLOAT) sqlite3_bind_double(stmt, i + 1, p.data.f);
            else if (p.type == VAL_STR) sqlite3_bind_text(stmt, i + 1, p.data.s, -1, SQLITE_TRANSIENT);
            else if (p.type == VAL_NIL) sqlite3_bind_null(stmt, i + 1);
          }
        }

        Value res_arr = val_arr();
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          Value row = val_obj();
          for (int i = 0; i < cols; i++) {
            const char *name = sqlite3_column_name(stmt, i);
            int type = sqlite3_column_type(stmt, i);
            Value val;
            if (type == SQLITE_INTEGER)
              val = val_int(sqlite3_column_int64(stmt, i));
            else if (type == SQLITE_FLOAT)
              val = val_float(sqlite3_column_double(stmt, i));
            else if (type == SQLITE_TEXT)
              val = val_str(nr_strdup((const char *)sqlite3_column_text(stmt, i)));
            else
              val = val_nil();
            set_field(row, name, val);
          }
          nr_rt_push(res_arr, row);
        }
        sqlite3_finalize(stmt);
        return res_arr;
      }

      // Resolve global function
      func_val = env_get_by_hash(env, full_name, node->data.call.hash);
      if (func_val.type == VAL_NIL) {
        func_val = env_get(env, full_name);
      }

      // Fallback to native FFI
      if (func_val.type == VAL_NIL) {
        for (int i = 0; i < dl_handle_count; i++) {
          void *ptr = dlsym(dl_handles[i], full_name);
          if (ptr) {
            return call_native_func(ptr, node, env);
          }
        }
      }
    }
  } else if (node->type == AST_FUNC_DECL) {
    func_val = val_func(node, env);
  }

  if (func_val.type == VAL_FUNC) {
    AstNode *decl = func_val.data.func.decl;
    // printf("DEBUG: Calling function %s, local_count=%d\n", decl->data.func_decl.name, decl->data.func_decl.local_count);
    Environment *call_env = env_new(func_val.data.func.closure, decl->data.func_decl.local_count);
    
    if (obj.type != VAL_NIL && !is_sugar)
      env_define(call_env, "self", obj);

    // Initialize params
    for (int i = 0; i < decl->data.func_decl.param_count; i++) {
      if (decl->data.func_decl.param_defaults && decl->data.func_decl.param_defaults[i])
        call_env->slots[i] = eval(decl->data.func_decl.param_defaults[i], env);
      else
        call_env->slots[i] = val_nil();
    }

    int pos_idx = 0;
    if (is_sugar && pos_idx < decl->data.func_decl.param_count) {
      call_env->slots[pos_idx++] = obj;
    }

    // Initialize params with defaults
    for (int i = 0; i < decl->data.func_decl.param_count; i++) {
      if (decl->data.func_decl.param_defaults && decl->data.func_decl.param_defaults[i])
        call_env->slots[i] = eval(decl->data.func_decl.param_defaults[i], env);
      else if (i >= pos_idx)
        call_env->slots[i] = val_nil();
    }

    if (node->type == AST_CALL) {
      // Positional arguments
      for (int i = 0; i < node->data.call.arg_count; i++) {
        if (node->data.call.arg_names && node->data.call.arg_names[i]) continue;
        if (pos_idx < decl->data.func_decl.param_count) {
          Value arg = eval(node->data.call.args[i], env);
          if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
          call_env->slots[pos_idx++] = arg;
        }
      }

      // Named arguments
      for (int i = 0; i < node->data.call.arg_count; i++) {
        if (node->data.call.arg_names && node->data.call.arg_names[i]) {
          char *name = node->data.call.arg_names[i];
          for (int j = 0; j < decl->data.func_decl.param_count; j++) {
            if (strcmp(decl->data.func_decl.params[j], name) == 0) {
              Value arg = eval(node->data.call.args[i], env);
              if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
              call_env->slots[j] = arg;
              break;
            }
          }
        }
      }

      // Variadic arguments
      if (decl->data.func_decl.is_variadic) {
        int variadic_idx = decl->data.func_decl.param_count - 1;
        int variadic_arg_count = node->data.call.arg_count - (variadic_idx - (is_sugar ? 1 : 0));
        if (variadic_arg_count < 0) variadic_arg_count = 0;

        Value var_arr = val_arr();
        for (int i = 0; i < variadic_arg_count; i++) {
          int arg_idx = (variadic_idx - (is_sugar ? 1 : 0)) + i;
          if (arg_idx < node->data.call.arg_count) {
             Value v = eval(node->data.call.args[arg_idx], env);
             if (v.type == VAL_RETURN) v = *v.data.return_val;
             array_push(var_arr.data.arr, v);
          }
        }
        call_env->slots[variadic_idx] = var_arr;
      }
    }

    Value res = eval(decl->data.func_decl.body, call_env);
    env_free(call_env);
    if (res.type == VAL_RETURN) return *res.data.return_val;
    return res;
  }

  char err_buf[256];
  snprintf(err_buf, sizeof(err_buf), "'%s' is not a function", (node->type == AST_CALL) ? node->data.call.name : "declaration");
  report_runtime_error(node, env, "TYPE", err_buf);
  return val_nil();
}

Value eval(AstNode *node, Environment *env) {
  if (!node)
    return val_nil();

#if defined(__GNUC__) || defined(__clang__)
  static void *dispatch_table[] = {
      [AST_PROGRAM] = &&do_program,
      [AST_FUNC_DECL] = &&do_func_decl,
      [AST_ASSIGN] = &&do_assign,
      [AST_VAR_REF] = &&do_var_ref,
      [AST_LITERAL_INT] = &&do_lit_int,
      [AST_LITERAL_FLOAT] = &&do_lit_float,
      [AST_LITERAL_STR] = &&do_lit_str,
      [AST_LITERAL_BOOL] = &&do_lit_bool,
      [AST_LITERAL_NULL] = &&do_lit_nil,
      [AST_BINARY] = &&do_binary,
      [AST_CALL] = &&do_call,
      [AST_RETURN] = &&do_return,
      [AST_IF] = &&do_if,
      [AST_WHILE] = &&do_while,
      [AST_BREAK] = &&do_break,
      [AST_CONTINUE] = &&do_continue,
      [AST_PASS] = &&do_pass,
      [AST_ERROR] = &&do_error,
      [AST_FOR] = &&do_for,
      [AST_OBJECT] = &&do_obj,
      [AST_ARRAY] = &&do_arr,
      [AST_INDEX] = &&do_index,
  };
  if (node->type < (sizeof(dispatch_table) / sizeof(void *)) &&
      dispatch_table[node->type]) {
    goto *dispatch_table[node->type];
  }
#endif

  switch (node->type) {
  case AST_PROGRAM:
  do_program: {
    Value last = val_nil();
    for (int i = 0; i < node->data.program.count; i++) {
      last = eval(node->data.program.statements[i], env);
      if (last.type == VAL_RETURN || last.type == VAL_BREAK ||
          last.type == VAL_CONTINUE)
        return last;
    }
    return last;
  }
  case AST_FUNC_DECL:
  do_func_decl: {
    Value f = val_func(node, env);
    if (strcmp(node->data.func_decl.name, "anonymous") != 0) {
      env_define(env, node->data.func_decl.name, f);
    }
    return f;
  }
  case AST_ASSIGN:
  do_assign: {
    Value v = eval(node->data.assign.value, env);
    if (v.type == VAL_RETURN)
      v = *v.data.return_val;

    if (node->data.assign.slot != -1 && env->slots) {
      env->slots[node->data.assign.slot] = v;
      if (node->data.assign.target) {
        env_define(env, node->data.assign.target, v);
      }
      return v;
    }

    char *target = nr_strdup(node->data.assign.target);
    char *dot = strchr(target, '.');
    if (dot) {
      *dot = '\0';
      char *obj_name = target;
      char *field_name = dot + 1;
      Value obj = env_get(env, obj_name);
      if (obj.type == VAL_OBJ) {
        int found = 0;
        for (int i = 0; i < obj.data.obj->count; i++) {
          if (strcmp(obj.data.obj->keys[i], field_name) == 0) {
            *obj.data.obj->values[i] = v;
            found = 1;
            break;
          }
        }
        if (!found) {
          if (obj.data.obj->count >= obj.data.obj->capacity) {
            obj.data.obj->capacity *= 2;
            obj.data.obj->keys =
                nr_realloc(obj.data.obj->keys,
                           sizeof(char *) * (obj.data.obj->capacity / 2),
                           sizeof(char *) * obj.data.obj->capacity);
            obj.data.obj->values =
                nr_realloc(obj.data.obj->values,
                           sizeof(Value *) * (obj.data.obj->capacity / 2),
                           sizeof(Value *) * obj.data.obj->capacity);
          }
          obj.data.obj->keys[obj.data.obj->count] = nr_strdup(field_name);
          obj.data.obj->values[obj.data.obj->count] = nr_malloc(sizeof(Value));

          *obj.data.obj->values[obj.data.obj->count] = v;
          obj.data.obj->count++;
        }
      }
    } else {
      env_assign(env, target, v);
    }
    return v;
  }
  case AST_VAR_REF:
  do_var_ref: {
    if (node->data.var_ref.slot != -1 && env->slots) {
      return env->slots[node->data.var_ref.slot];
    }
    char *full_name = nr_strdup(node->data.var_ref.name);
    char *dot = strchr(full_name, '.');
    Value v = val_nil();
    if (dot) {
      *dot = '\0';
      Value obj = env_get(env, full_name);
      v = get_dot_value(obj, dot + 1);
    } else {
      v = env_get_by_hash(env, full_name, node->data.var_ref.hash);
    }
    return v;
  }
  case AST_LITERAL_INT:
  do_lit_int:
    return val_int(node->data.int_val);
  case AST_LITERAL_FLOAT:
  do_lit_float:
    return val_float(node->data.float_val);
  case AST_LITERAL_STR:
  do_lit_str:
    return val_str(node->data.str_val);
  case AST_LITERAL_BOOL:
  do_lit_bool:
    return val_bool(node->data.int_val);
  case AST_LITERAL_NULL:
  do_lit_nil:
    return val_nil();
  case AST_BINARY:
  do_binary: {
    Value left;
    AstNode *l_node = node->data.binary.left;
    if (l_node->type == AST_VAR_REF && l_node->data.var_ref.slot != -1 &&
        env->slots) {
      left = env->slots[l_node->data.var_ref.slot];
    } else if (l_node->type == AST_LITERAL_INT) {
      left = (Value){.type = VAL_INT, .data.i = l_node->data.int_val};
    } else {
      left = EVAL_FAST(l_node, env);
      if (left.type == VAL_RETURN || left.type == VAL_BREAK ||
          left.type == VAL_CONTINUE)
        return left;
    }

    BinOp op = node->data.binary.op;
    if (op == OP_AND) {
      if (!is_truthy(left))
        return left;
      return eval(node->data.binary.right, env);
    }
    if (op == OP_OR) {
      if (is_truthy(left))
        return left;
      return eval(node->data.binary.right, env);
    }
    if (op == OP_NOT)
      return val_bool(!is_truthy(left));

    Value right;
    AstNode *r_node = node->data.binary.right;
    if (r_node) {
      if (r_node->type == AST_VAR_REF && r_node->data.var_ref.slot != -1 &&
          env->slots) {
        right = env->slots[r_node->data.var_ref.slot];
      } else if (r_node->type == AST_LITERAL_INT) {
        right = (Value){.type = VAL_INT, .data.i = r_node->data.int_val};
      } else {
        right = EVAL_FAST(r_node, env);
        if (right.type == VAL_RETURN || right.type == VAL_BREAK ||
            right.type == VAL_CONTINUE)
          return right;
      }
    } else
      right = val_nil();

    if (op == OP_EQ) {
      if (left.type != right.type)
        return val_bool(0);
      switch (left.type) {
      case VAL_NIL:
        return val_bool(1);
      case VAL_INT:
        return val_bool(left.data.i == right.data.i);
      case VAL_FLOAT:
        return val_bool(left.data.f == right.data.f);
      case VAL_STR:
        return val_bool(strcmp(left.data.s, right.data.s) == 0);
      case VAL_BOOL:
        return val_bool(left.data.i == right.data.i);
      case VAL_OBJ:
        return val_bool(left.data.obj == right.data.obj);
      case VAL_ARR:
        return val_bool(left.data.arr == right.data.arr);
      default:
        return val_bool(0);
      }
    }
    if (op == OP_NEQ) {
      if (left.type != right.type)
        return val_bool(1);
      switch (left.type) {
      case VAL_NIL:
        return val_bool(0);
      case VAL_INT:
        return val_bool(left.data.i != right.data.i);
      case VAL_FLOAT:
        return val_bool(left.data.f != right.data.f);
      case VAL_STR:
        return val_bool(strcmp(left.data.s, right.data.s) != 0);
      case VAL_BOOL:
        return val_bool(left.data.i != right.data.i);
      case VAL_OBJ:
        return val_bool(left.data.obj != right.data.obj);
      case VAL_ARR:
        return val_bool(left.data.arr != right.data.arr);
      default:
        return val_bool(1);
      }
    }

    if ((left.type == VAL_INT || left.type == VAL_FLOAT) &&
        (right.type == VAL_INT || right.type == VAL_FLOAT)) {
      double l = (left.type == VAL_FLOAT) ? left.data.f : (double)left.data.i;
      double r =
          (right.type == VAL_FLOAT) ? right.data.f : (double)right.data.i;
      if (left.type == VAL_FLOAT || right.type == VAL_FLOAT || op == OP_DIV) {
        switch (op) {
        case OP_ADD:
          return val_float(l + r);
        case OP_SUB:
          return val_float(l - r);
        case OP_MUL:
          return val_float(l * r);
        case OP_DIV:
          if (r == 0)
            report_runtime_error(node, env, "MATH", "Division by zero");
          return val_float(l / r);
        case OP_LT:
          return val_bool(l < r);
        case OP_GT:
          return val_bool(l > r);
        default:
          break;
        }
      } else {
        long long il = (long long)l;
        long long ir = (long long)r;
        switch (op) {
        case OP_ADD:
          return (Value){.type = VAL_INT, .data.i = il + ir};
        case OP_SUB:
          return (Value){.type = VAL_INT, .data.i = il - ir};
        case OP_MUL:
          return (Value){.type = VAL_INT, .data.i = il * ir};
        case OP_LT:
          return (Value){.type = VAL_BOOL, .data.i = il < ir};
        case OP_GT:
          return (Value){.type = VAL_BOOL, .data.i = il > ir};
        case OP_LE:
          return (Value){.type = VAL_BOOL, .data.i = il <= ir};
        case OP_GE:
          return (Value){.type = VAL_BOOL, .data.i = il >= ir};
        case OP_DIV:
          if (ir == 0)
            report_runtime_error(node, env, "MATH", "Division by zero");
          return (Value){.type = VAL_INT, .data.i = il / ir};
        default:
          break;
        }
      }
    }
    if (left.type == VAL_STR && op == OP_ADD) {
      Value sr = val_to_str(right);
      int len_l = strlen(left.data.s);
      int len_r = strlen(sr.data.s);
      char *res = nr_malloc(len_l + len_r + 1);
      memcpy(res, left.data.s, len_l);
      memcpy(res + len_l, sr.data.s, len_r);
      res[len_l + len_r] = '\0';
      return (Value){.type = VAL_STR, .length = len_l + len_r, .data.s = res};
    }
    if (right.type == VAL_STR && op == OP_ADD) {
      Value sl = val_to_str(left);
      int len_l = strlen(sl.data.s);
      int len_r = strlen(right.data.s);
      char *res = nr_malloc(len_l + len_r + 1);
      memcpy(res, sl.data.s, len_l);
      memcpy(res + len_l, right.data.s, len_r);
      res[len_l + len_r] = '\0';
      return (Value){.type = VAL_STR, .length = len_l + len_r, .data.s = res};
    }
    return val_nil();
  }
  case AST_CALL:
  do_call:
    return eval_call(node, env);

  case AST_ERROR:
  do_error: {
    Value msg = eval(node->data.error_expr.message, env);
    if (msg.type == VAL_RETURN)
      msg = *msg.data.return_val;
    return val_error(msg.type == VAL_STR ? msg.data.s : "error");
  }
  case AST_RETURN:
  do_return: {
    Value v = eval(node->data.ret.value, env);
    if (v.type == VAL_RETURN)
      return v;
    return val_return(v);
  }
  case AST_IF:
  do_if: {
    Value cond = eval(node->data.if_stmt.condition, env);
    if (cond.type == VAL_RETURN || cond.type == VAL_BREAK ||
        cond.type == VAL_CONTINUE)
      return cond;
    if (cond.type == VAL_BOOL ? cond.data.i : (cond.type != VAL_NIL)) {
      return eval(node->data.if_stmt.then_branch, env);
    } else if (node->data.if_stmt.else_branch) {
      return eval(node->data.if_stmt.else_branch, env);
    }
    return val_nil();
  }
  case AST_WHILE:
  do_while: {
    while (1) {
      Value cond = eval(node->data.while_stmt.condition, env);
      if (cond.type == VAL_RETURN || cond.type == VAL_BREAK ||
          cond.type == VAL_CONTINUE)
        return cond;
      if (!is_truthy(cond))
        break;

      Value res = eval(node->data.while_stmt.body, env);
      if (res.type == VAL_RETURN)
        return res;
      if (res.type == VAL_BREAK)
        break;
      if (res.type == VAL_CONTINUE)
        continue;
    }
    return val_nil();
  }
  case AST_BREAK:
  do_break:
    return (Value){.type = VAL_BREAK};
  case AST_CONTINUE:
  do_continue:
    return (Value){.type = VAL_CONTINUE};
  case AST_PASS:
  do_pass:
    return val_nil();
  case AST_FOR:
  do_for: {
    Value iter = eval(node->data.for_stmt.iterable, env);
    if (iter.type == VAL_RETURN)
      iter = *iter.data.return_val;
    if (iter.type != VAL_ARR)
      report_runtime_error(node, env, "TYPE", "Cannot iterate over non-array");
    char *var_name = node->data.for_stmt.alias ? node->data.for_stmt.alias
                                                : node->data.for_stmt.var;
    for (int i = 0; i < iter.data.arr->count; i++) {
      if (node->data.for_stmt.slot != -1 && env->slots) {
        Value val = *iter.data.arr->elements[i];
        env->slots[node->data.for_stmt.slot] = val;
        env_define(env, var_name, val);
      } else {
        env_define(env, var_name, *iter.data.arr->elements[i]);
      }
      Value res = eval(node->data.for_stmt.body, env);
      if (res.type == VAL_RETURN)
        return res;
      if (res.type == VAL_BREAK)
        break;
      if (res.type == VAL_CONTINUE)
        continue;
    }
    return val_nil();
  }
  case AST_OBJECT:
  do_obj: {
    Value obj = val_obj();
    AstField *f = node->data.object.fields;
    while (f) {
      if (obj.data.obj->count >= obj.data.obj->capacity) {
        int old_cap = obj.data.obj->capacity;
        obj.data.obj->capacity *= 2;
        obj.data.obj->keys =
            nr_realloc(obj.data.obj->keys, sizeof(char *) * old_cap,
                       sizeof(char *) * obj.data.obj->capacity);
        obj.data.obj->values =
            nr_realloc(obj.data.obj->values, sizeof(Value *) * old_cap,
                       sizeof(Value *) * obj.data.obj->capacity);
      }
      obj.data.obj->keys[obj.data.obj->count] = nr_strdup(f->name);
      obj.data.obj->values[obj.data.obj->count] = nr_malloc(sizeof(Value));

      Value val = eval(f->value, env);
      if (val.type == VAL_RETURN)
        val = *val.data.return_val;
      *obj.data.obj->values[obj.data.obj->count] = val;
      obj.data.obj->count++;
      f = f->next;
    }
    return obj;
  }
  case AST_ARRAY:
  do_arr: {
    Value arr = val_arr();
    for (int i = 0; i < node->data.array.count; i++) {
      if (arr.data.arr->count >= arr.data.arr->capacity) {
        int old_cap = arr.data.arr->capacity;
        arr.data.arr->capacity *= 2;
        arr.data.arr->elements =
            nr_realloc(arr.data.arr->elements, sizeof(Value *) * old_cap,
                       sizeof(Value *) * arr.data.arr->capacity);
      }
      arr.data.arr->elements[arr.data.arr->count] = nr_malloc(sizeof(Value));

      Value val = eval(node->data.array.elements[i], env);
      if (val.type == VAL_RETURN)
        val = *val.data.return_val;
      *arr.data.arr->elements[arr.data.arr->count] = val;
      arr.data.arr->count++;
    }
    return arr;
  }
  case AST_INDEX:
  do_index: {
    Value obj = eval(node->data.index.object, env);
    Value idx = eval(node->data.index.index, env);
    if (obj.type == VAL_RETURN)
      obj = *obj.data.return_val;
    if (idx.type == VAL_RETURN)
      idx = *idx.data.return_val;
    if (obj.type == VAL_ARR) {
      if (idx.type == VAL_STR && strcmp(idx.data.s, "length") == 0) {
        return val_int(obj.data.arr->count);
      }
      if (idx.type != VAL_INT)
        report_runtime_error(node, env, "TYPE", "Array index must be integer");
      if (idx.data.i < 0 || idx.data.i >= obj.data.arr->count)
        report_runtime_error(node, env, "BOUNDS", "Array index out of bounds");
      return *obj.data.arr->elements[idx.data.i];
    } else if (obj.type == VAL_STR) {
      if (idx.type == VAL_STR && strcmp(idx.data.s, "length") == 0) {
        return val_int(strlen(obj.data.s));
      }
      if (idx.type != VAL_INT)
        report_runtime_error(node, env, "TYPE", "String index must be integer");
      int len = strlen(obj.data.s);
      if (idx.data.i < 0 || idx.data.i >= len)
        report_runtime_error(node, env, "BOUNDS", "String index out of bounds");
      char *s = nr_malloc(2);
      s[0] = obj.data.s[idx.data.i];
      s[1] = '\0';
      return val_str(s);
    } else if (obj.type == VAL_OBJ) {
      char *key_str;
      if (idx.type == VAL_STR)
        key_str = idx.data.s;
      else {
        char buf[64];
        if (idx.type == VAL_INT)
          snprintf(buf, sizeof(buf), "%lld", idx.data.i);
        else if (idx.type == VAL_FLOAT)
          snprintf(buf, sizeof(buf), "%g", idx.data.f);
        else if (idx.type == VAL_BOOL)
          snprintf(buf, sizeof(buf), "%s", idx.data.i ? "true" : "false");
        else
          snprintf(buf, sizeof(buf), "nil");
        key_str = nr_strdup(buf);
      }
      return get_dot_value(obj, key_str);
    }
    return val_nil();
  }
  case AST_DESTRUCTURING: {
    Value val = eval(node->data.destruct.value, env);
    if (val.type == VAL_RETURN)
      val = *val.data.return_val;
    if (val.type != VAL_OBJ)
      report_runtime_error(node, env, "TYPE",
                           "Destructuring target must be an object");
    AstField *f = node->data.destruct.target->data.object.fields;
    while (f) {
      Value field_val = val_nil();
      for (int i = 0; i < val.data.obj->count; i++) {
        if (strcmp(val.data.obj->keys[i], f->name) == 0) {
          field_val = *val.data.obj->values[i];
          break;
        }
      }
      char *target_name = f->alias ? f->alias : f->name;
      env_define(env, target_name, field_val);
      f = f->next;
    }
    return val;
  }
  case AST_INDEX_ASSIGN: {
    Value obj = eval(node->data.index_assign.object, env);
    Value idx = eval(node->data.index_assign.index, env);
    Value val = eval(node->data.index_assign.value, env);
    if (obj.type == VAL_RETURN)
      obj = *obj.data.return_val;
    if (idx.type == VAL_RETURN)
      idx = *idx.data.return_val;
    if (val.type == VAL_RETURN)
      val = *val.data.return_val;
    if (obj.type == VAL_ARR) {
      if (idx.type != VAL_INT)
        report_runtime_error(node, env, "TYPE", "Array index must be integer");
      if (idx.data.i < 0 || idx.data.i >= obj.data.arr->count)
        report_runtime_error(node, env, "BOUNDS", "Array index out of bounds");
      *obj.data.arr->elements[idx.data.i] = val;
    } else if (obj.type == VAL_OBJ) {
      char *key_str;
      if (idx.type == VAL_STR)
        key_str = idx.data.s;
      else {
        char buf[64];
        if (idx.type == VAL_INT)
          snprintf(buf, sizeof(buf), "%lld", idx.data.i);
        else if (idx.type == VAL_FLOAT)
          snprintf(buf, sizeof(buf), "%g", idx.data.f);
        else if (idx.type == VAL_BOOL)
          snprintf(buf, sizeof(buf), "%s", idx.data.i ? "true" : "false");
        else
          snprintf(buf, sizeof(buf), "nil");
        key_str = nr_strdup(buf);
      }
      eval_set_field(obj.data.obj, key_str, val);
    }
    return val;
  }
  case AST_IMPORT: {
    char *clean_path = node->data.import_stmt.path;
    char full_path[512];
    char *source = NULL;
    int is_lib = node->data.import_stmt.is_library;

    if (is_lib) {
      // 1. Try standard library
      snprintf(full_path, sizeof(full_path), "%s/%s.nr", nira_std_lib_path,
               clean_path);
      source = nr_read_file_internal(full_path);

      // 2. Try local .nira/libs
      if (!source) {
        snprintf(full_path, sizeof(full_path), ".nira/libs/%s/%s.nr",
                 clean_path, clean_path);
        source = nr_read_file_internal(full_path);
      }
      if (!source) {
        snprintf(full_path, sizeof(full_path), ".nira/libs/%s.nr", clean_path);
        source = nr_read_file_internal(full_path);
      }

      // 3. Try global libs
      if (!source) {
        snprintf(full_path, sizeof(full_path), "%s/%s/%s.nr",
                 nira_global_libs_path, clean_path, clean_path);
        source = nr_read_file_internal(full_path);
      }
      if (!source) {
        snprintf(full_path, sizeof(full_path), "%s/%s.nr",
                 nira_global_libs_path, clean_path);
        source = nr_read_file_internal(full_path);
      }
    } else {
      // 1. Try include paths (workspace)
      for (int i = 0; i < include_path_count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s.nr", include_paths[i],
                 clean_path);
        source = nr_read_file_internal(full_path);
        if (source)
          break;
      }

      // 2. Try current directory
      if (!source) {
        snprintf(full_path, sizeof(full_path), "%s.nr", clean_path);
        source = nr_read_file_internal(full_path);
      }
    }

    if (!source) {
      char buf[512];
      snprintf(buf, sizeof(buf), "Could not find module '%s' (is_library=%d)",
               clean_path, is_lib);
      report_runtime_error(node, env, "IMPORT", buf);
      return val_nil();
    }
    Lexer lex;
    lexer_init(&lex, source);
    Parser p;
    parser_init(&p, &lex, full_path);
    AstNode *imported_program = parse_program(&p);
    nr_resolve(imported_program);
    Environment *root = env;
    while (root->parent)
      root = root->parent;
    Environment *import_env = env_new(root, 0);
    import_env->source = source;
    import_env->filename = nr_strdup(full_path);
    eval(imported_program, import_env);
    char *final_name = node->data.import_stmt.alias;
    if (!final_name && node->data.import_stmt.symbol_count == 0) {
      char *slash = strrchr(clean_path, '/');
      final_name = slash ? slash + 1 : clean_path;
    }
    if (final_name) {
      Value mod_obj = val_obj();
      for (int i = 0; i < import_env->capacity; i++) {
        Variable *v = import_env->table[i];
        while (v) {
          if (mod_obj.data.obj->count >= mod_obj.data.obj->capacity) {
            int old_cap = mod_obj.data.obj->capacity;
            mod_obj.data.obj->capacity *= 2;
            mod_obj.data.obj->keys =
                nr_realloc(mod_obj.data.obj->keys, sizeof(char *) * old_cap,
                           sizeof(char *) * mod_obj.data.obj->capacity);
            mod_obj.data.obj->values =
                nr_realloc(mod_obj.data.obj->values, sizeof(Value *) * old_cap,
                           sizeof(Value *) * mod_obj.data.obj->capacity);
          }
          mod_obj.data.obj->keys[mod_obj.data.obj->count] = nr_strdup(v->name);
          mod_obj.data.obj->values[mod_obj.data.obj->count] =
              nr_malloc(sizeof(Value));
          *mod_obj.data.obj->values[mod_obj.data.obj->count] = v->value;
          mod_obj.data.obj->count++;
          v = v->next;
        }
      }
      env_define(env, final_name, mod_obj);
    } else if (node->data.import_stmt.symbol_count > 0) {
      for (int i = 0; i < node->data.import_stmt.symbol_count; i++) {
        Value v = env_get(import_env, node->data.import_stmt.symbols[i]);
        env_define(env, node->data.import_stmt.symbols[i], v);
      }
    } else {
      for (int i = 0; i < import_env->capacity; i++) {
        Variable *v = import_env->table[i];
        while (v) {
          env_define(env, v->name, v->value);
          v = v->next;
        }
      }
    }
    return val_nil();
  }
  case AST_EXPORT: {
    return eval(node->data.func_decl.body, env);
  }
  case AST_NATIVE: {
    if (node->data.native_stmt.code) {
      char hash[64];
      unsigned int h = 0;
      for (int i = 0; node->data.native_stmt.code[i]; i++)
        h = h * 31 + node->data.native_stmt.code[i];
      snprintf(hash, sizeof(hash), "native_%u", h);

      char c_file[256];
      char so_file[256];
      snprintf(c_file, sizeof(c_file), ".nira/cache/%s.c", hash);
#ifdef __APPLE__
      snprintf(so_file, sizeof(so_file), ".nira/cache/%s.dylib", hash);
#else
      snprintf(so_file, sizeof(so_file), ".nira/cache/%s.so", hash);
#endif

      struct stat st;
      if (stat(so_file, &st) != 0) {
        mkdir(".nira", 0755);
        mkdir(".nira/cache", 0755);
        FILE *f = fopen(c_file, "w");
        if (f) {
          fprintf(f, "#include \"evaluator.h\"\n");
          for (int i = 0; i < node->data.native_stmt.header_count; i++) {
            char *h = node->data.native_stmt.headers[i];
            if (h[0] == '<' || h[0] == '"')
              fprintf(f, "#include %s\n", h);
            else
              fprintf(f, "#include \"%s\"\n", h);
          }
          fprintf(f, "%s\n", node->data.native_stmt.code);
          fclose(f);

          // Compute include path from nira_std_lib_path (sibling of lib/)
          char nira_include_path[1024];
          strncpy(nira_include_path, nira_std_lib_path,
                  sizeof(nira_include_path));
          char *lib_suffix = strrchr(nira_include_path, '/');
          if (lib_suffix) {
            strcpy(lib_suffix + 1, "include");
          } else {
            strcpy(nira_include_path, "include");
          }

          char cmd[2048];
          char link_flags[512] = "";
          for (int i = 0; i < node->data.native_stmt.link_count; i++) {
            strcat(link_flags, node->data.native_stmt.links[i]);
            strcat(link_flags, " ");
          }
#ifdef __APPLE__
          snprintf(
              cmd, sizeof(cmd),
              "clang -shared -fPIC -I%s %s -undefined dynamic_lookup -o %s %s",
              nira_include_path, link_flags, so_file, c_file);
#else
          snprintf(cmd, sizeof(cmd), "clang -shared -fPIC -I%s %s -o %s %s",
                   nira_include_path, link_flags, so_file, c_file);
#endif
          int res = system(cmd);
          if (res != 0) {
            report_runtime_error(node, env, "FFI",
                                 "Failed to compile native block");
            return val_nil();
          }
        }
      }

      void *handle = dlopen(so_file, RTLD_NOW | RTLD_GLOBAL);
      if (handle) {
        if (dl_handle_count < 64)
          dl_handles[dl_handle_count++] = handle;
      } else {
        report_runtime_error(node, env, "FFI", dlerror());
      }
    }
    return val_nil();
  }
  case AST_EXTERN: {
    if (node->data.extern_stmt.is_header) {
      char *p = node->data.extern_stmt.path;
      if (strstr(p, ".so") || strstr(p, ".dylib") || strstr(p, ".dll") ||
          strstr(p, "lib")) {
        void *handle = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
        if (!handle) {
          // Try common paths or just name
          handle = dlopen(p, RTLD_NOW | RTLD_GLOBAL);
        }
        if (handle) {
          if (dl_handle_count < 64)
            dl_handles[dl_handle_count++] = handle;
        }
      }
    } else {
      // For interpreter, we can't easily call raw C functions without libffi.
      // We'll assume the function is provided via a 'native' block or
      // is already in the dl_handles and matches Nira's signature.
      // We define it as a nil for now or a proxy if we want.
    }
    return val_nil();
  }
  default:
    return val_nil();
  }
}
