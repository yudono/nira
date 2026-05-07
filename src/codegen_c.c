#include "../include/ast.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

void codegen_c_node(AstNode *node, FILE *out);

const char* include_paths[16];
int include_path_count = 0;

void nr_add_include_path(const char* path) {
    if (include_path_count < 16) include_paths[include_path_count++] = strdup(path);
}

extern char* read_file(const char* path);
static char* nr_read_file(const char* path) {
    FILE* file = fopen(path, "rb"); if (!file) return NULL;
    fseek(file, 0L, SEEK_END); size_t fileSize = ftell(file); rewind(file);
    char* buffer = (char*)malloc(fileSize + 1); fread(buffer, 1, fileSize, file); buffer[fileSize] = 0; fclose(file);
    return buffer;
}

static int global_lambda_count = 0;
static char* function_names[8192];
static int function_count = 0;
static char* current_mod = NULL;
static char* current_pref = NULL;

static char* global_vars[8192];
static int global_var_count = 0;

static char* generated_modules[128];
static int generated_module_count = 0;
static char* collected_modules[128];
static int collected_module_count = 0;
static AstNode* module_progs[128];

static int is_function(const char* name) {
    for (int i=0; i<function_count; i++) if (strcmp(function_names[i], name) == 0) return 1;
    return 0;
}

static int is_global(const char* name) {
    for (int i=0; i<global_var_count; i++) if (strcmp(global_vars[i], name) == 0) return 1;
    return 0;
}

static void print_runtime(FILE* out) {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <unistd.h>\n#include <sys/stat.h>\n#include <time.h>\n#include <sqlite3.h>\n#include <regex.h>\n#include <ctype.h>\n");
    fprintf(out, "#ifdef _WIN32\n  #include <winsock2.h>\n  #include <ws2tcpip.h>\n  #pragma comment(lib, \"ws2_32.lib\")\n  #define close closesocket\n  #define mkdir(p, m) _mkdir(p)\n#else\n  #include <sys/socket.h>\n  #include <netinet/in.h>\n  #include <arpa/inet.h>\n#endif\n\nint nr_argc; char** nr_argv;\n");
    fprintf(out, "typedef enum { VAL_NIL, VAL_INT, VAL_FLOAT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_ERROR } ValueType;\n");
    fprintf(out, "typedef struct ArenaBlock { void* ptr; struct ArenaBlock* next; } ArenaBlock;\n");
    fprintf(out, "typedef struct { ArenaBlock* blocks; } Arena; Arena* nr_arena;\n");
    fprintf(out, "void* nr_alloc(size_t sz) { void* p = malloc(sz); ArenaBlock* b = malloc(sizeof(ArenaBlock)); b->ptr = p; b->next = nr_arena->blocks; nr_arena->blocks = b; return p; }\n");
    fprintf(out, "void* nr_checkpoint() { return nr_arena->blocks; }\n");
    fprintf(out, "void nr_rollback(void* cp) { ArenaBlock* curr = nr_arena->blocks; while(curr && curr != cp) { ArenaBlock* next = curr->next; free(curr->ptr); free(curr); curr = next; } nr_arena->blocks = cp; }\n");
    fprintf(out, "void nr_arena_clear() { nr_rollback(NULL); }\n");
    fprintf(out, "char* nr_strdup(const char* s) { char* d = nr_alloc(strlen(s)+1); strcpy(d, s); return d; }\n");
    fprintf(out, "struct Value; typedef struct Value { ValueType type; union { int i; double f; char* s; struct { char** keys; struct Value** values; int count; int capacity; }* obj; struct { struct Value** elements; int count; int capacity; }* arr; void* func_ptr; } data; } Value;\n\n");
    fprintf(out, "Value val_nil() { return (Value){.type = VAL_NIL}; }\nValue val_int(int i) { return (Value){.type = VAL_INT, .data.i = i}; }\nValue val_float(double f) { return (Value){.type = VAL_FLOAT, .data.f = f}; }\nValue val_bool(bool b) { return (Value){.type = VAL_BOOL, .data.i = b ? 1 : 0}; }\nValue val_str(const char* s) { return (Value){.type = VAL_STR, .data.s = nr_strdup(s)}; }\nValue val_error(const char* m) { return (Value){.type = VAL_ERROR, .data.s = nr_strdup(m)}; }\nValue val_func(void* ptr) { return (Value){.type = VAL_FUNC, .data.func_ptr = ptr}; }\nbool is_truthy(Value v) { if (v.type == VAL_NIL) return false; if (v.type == VAL_BOOL || v.type == VAL_INT) return v.data.i != 0; if (v.type == VAL_FLOAT) return v.data.f != 0.0; return true; }\n\n");
    fprintf(out, "Value val_obj() { Value v; v.type = VAL_OBJ; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 8; v.data.obj->keys = nr_alloc(sizeof(char*) * 8); v.data.obj->values = nr_alloc(sizeof(Value*) * 8); return v; }\n");
    fprintf(out, "Value val_arr() { Value v; v.type = VAL_ARR; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 8; v.data.arr->elements = nr_alloc(sizeof(Value*) * 8); return v; }\n\n");
    fprintf(out, "void set_field(Value obj, const char* key, Value val) { if (obj.type != VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) { if(strcmp(obj.data.obj->keys[i], key) == 0) { *obj.data.obj->values[i] = val; return; } } if(obj.data.obj->count >= obj.data.obj->capacity) { int old_cap = obj.data.obj->capacity; obj.data.obj->capacity *= 2; char** new_keys = nr_alloc(sizeof(char*) * obj.data.obj->capacity); Value** new_vals = nr_alloc(sizeof(Value*) * obj.data.obj->capacity); memcpy(new_keys, obj.data.obj->keys, sizeof(char*)*old_cap); memcpy(new_vals, obj.data.obj->values, sizeof(Value*)*old_cap); obj.data.obj->keys = new_keys; obj.data.obj->values = new_vals; } obj.data.obj->keys[obj.data.obj->count] = nr_strdup(key); obj.data.obj->values[obj.data.obj->count] = nr_alloc(sizeof(Value)); *obj.data.obj->values[obj.data.obj->count] = val; obj.data.obj->count++; }\n");
    fprintf(out, "Value get_field(Value obj, const char* key) { if (obj.type != VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) { if(strcmp(obj.data.obj->keys[i], key) == 0) return *obj.data.obj->values[i]; } return val_nil(); }\n\n");
    
    // Runtime functions - ORDER MATTERS
    fprintf(out, "Value nr_rt_push(Value arr, Value val) { if (arr.type != VAL_ARR) return val_nil(); if (arr.data.arr->count >= arr.data.arr->capacity) { int old_cap = arr.data.arr->capacity; arr.data.arr->capacity *= 2; Value** new_el = nr_alloc(sizeof(Value*) * arr.data.arr->capacity); memcpy(new_el, arr.data.arr->elements, sizeof(Value*)*old_cap); arr.data.arr->elements = new_el; } arr.data.arr->elements[arr.data.arr->count] = nr_alloc(sizeof(Value)); *arr.data.arr->elements[arr.data.arr->count] = val; arr.data.arr->count++; return val; }\n");
    fprintf(out, "Value nr_rt_obj_keys(Value obj) { if (obj.type != VAL_OBJ) return val_arr(); Value a = val_arr(); for(int i=0; i<obj.data.obj->count; i++) nr_rt_push(a, val_str(obj.data.obj->keys[i])); return a; }\n");
    fprintf(out, "Value nr_rt_pop(Value arr) { if (arr.type != VAL_ARR || arr.data.arr->count == 0) return val_nil(); arr.data.arr->count--; return *arr.data.arr->elements[arr.data.arr->count]; }\n");
    fprintf(out, "Value nr_rt_at(Value v, Value idx) { if (v.type == VAL_ARR && idx.type == VAL_INT) { int i = idx.data.i; if (i < 0 || i >= v.data.arr->count) return val_nil(); return *v.data.arr->elements[i]; } if (v.type == VAL_OBJ && idx.type == VAL_STR) return get_field(v, idx.data.s); if (v.type == VAL_STR && idx.type == VAL_INT) { int i = idx.data.i; if (i < 0 || i >= (int)strlen(v.data.s)) return val_nil(); char s[2] = {v.data.s[i], 0}; return val_str(s); } return val_nil(); }\n");
    fprintf(out, "Value nr_rt_set_at(Value v, Value idx, Value val) { if (v.type == VAL_ARR && idx.type == VAL_INT) { int i = idx.data.i; if (i >= 0 && i < v.data.arr->count) *v.data.arr->elements[i] = val; } else if (v.type == VAL_OBJ && idx.type == VAL_STR) set_field(v, idx.data.s, val); return val; }\n");
    fprintf(out, "Value nr_rt_to_int(Value v) { if (v.type == VAL_INT) return v; if (v.type == VAL_STR) return val_int((int)strtol(v.data.s, NULL, 10)); return val_int(0); }\n");
    fprintf(out, "void nr_rt_print(Value v) { if (v.type == VAL_INT) printf(\"%%d\\n\", v.data.i); else if (v.type == VAL_BOOL) printf(\"%%s\\n\", v.data.i ? \"true\" : \"false\"); else if (v.type == VAL_STR) printf(\"%%s\\n\", v.data.s); else if (v.type == VAL_ARR) printf(\"[Array]\\n\"); else if (v.type == VAL_OBJ) printf(\"[Object]\\n\"); else if (v.type == VAL_ERROR) printf(\"Error: %%s\\n\", v.data.s); else printf(\"nil\\n\"); fflush(stdout); }\n");
    fprintf(out, "Value nr_rt_len(Value v) { if (v.type == VAL_ARR) return val_int(v.data.arr->count); if (v.type == VAL_STR) return val_int(strlen(v.data.s)); return val_int(0); }\n");
    
    fprintf(out, "Value nr_rt_add(Value l, Value r) {\n");
    fprintf(out, "  if (l.type == VAL_STR || r.type == VAL_STR) {\n");
    fprintf(out, "    char buf_l[64], buf_r[64]; char *sl, *sr;\n");
    fprintf(out, "    if (l.type == VAL_STR) sl = l.data.s; else if (l.type == VAL_INT) { snprintf(buf_l, 64, \"%%d\", l.data.i); sl = buf_l; } else if (l.type == VAL_FLOAT) { snprintf(buf_l, 64, \"%%g\", l.data.f); sl = buf_l; } else sl = \"nil\";\n");
    fprintf(out, "    if (r.type == VAL_STR) sr = r.data.s; else if (r.type == VAL_INT) { snprintf(buf_r, 64, \"%%d\", r.data.i); sr = buf_r; } else if (r.type == VAL_FLOAT) { snprintf(buf_r, 64, \"%%g\", r.data.f); sr = buf_r; } else sr = \"nil\";\n");
    fprintf(out, "    char* res = nr_alloc(strlen(sl) + strlen(sr) + 1); strcpy(res, sl); strcat(res, sr); return val_str(res);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  if (l.type == VAL_FLOAT || r.type == VAL_FLOAT) {\n");
    fprintf(out, "    double lv = (l.type == VAL_FLOAT) ? l.data.f : (double)l.data.i;\n");
    fprintf(out, "    double rv = (r.type == VAL_FLOAT) ? r.data.f : (double)r.data.i;\n");
    fprintf(out, "    return val_float(lv + rv);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  return val_int(l.data.i + r.data.i);\n");
    fprintf(out, "}\n");

    fprintf(out, "Value nr_rt_eq(Value l, Value r) {\n");
    fprintf(out, "  if (l.type == r.type) {\n");
    fprintf(out, "    if (l.type == VAL_NIL) return val_bool(1);\n");
    fprintf(out, "    if (l.type == VAL_STR) return val_bool(strcmp(l.data.s, r.data.s) == 0);\n");
    fprintf(out, "    if (l.type == VAL_INT || l.type == VAL_BOOL) return val_bool(l.data.i == r.data.i);\n");
    fprintf(out, "    if (l.type == VAL_FLOAT) return val_bool(l.data.f == r.data.f);\n");
    fprintf(out, "    return val_bool(l.data.obj == r.data.obj);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  if ((l.type == VAL_INT || l.type == VAL_FLOAT) && (r.type == VAL_INT || r.type == VAL_FLOAT)) {\n");
    fprintf(out, "    double lv = (l.type == VAL_FLOAT) ? l.data.f : (double)l.data.i;\n    double rv = (r.type == VAL_FLOAT) ? r.data.f : (double)r.data.i;\n");
    fprintf(out, "    return val_bool(lv == rv);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  return val_bool(0);\n");
    fprintf(out, "}\n\n");

    fprintf(out, "static int is_safe_path(const char* path) { if (!path) return 0; if (strstr(path, \"..\")) return 0; return 1; }\n");
    fprintf(out, "Value nr_rt_read_file(Value path) { if (path.type != VAL_STR) return val_nil(); FILE* f = fopen(path.data.s, \"rb\"); if (!f) return val_nil(); fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); char* b = nr_alloc(sz + 1); fread(b, 1, sz, f); b[sz] = 0; fclose(f); return val_str(b); }\n");
    fprintf(out, "Value nr_rt_file_write(Value path, Value content) { if (path.type != VAL_STR || content.type != VAL_STR) return val_nil(); if (!is_safe_path(path.data.s)) return val_error(\"Path traversal detected\"); FILE* f = fopen(path.data.s, \"w\"); if (!f) return val_nil(); fputs(content.data.s, f); fclose(f); return val_int(1); }\n");
    fprintf(out, "Value nr_rt_file_exists(Value path) { if (path.type != VAL_STR) return val_bool(false); if (!is_safe_path(path.data.s)) return val_bool(false); struct stat st; return val_bool(stat(path.data.s, &st) == 0); }\n");
    fprintf(out, "Value nr_rt_file_delete(Value path) { if (path.type != VAL_STR) return val_nil(); if (!is_safe_path(path.data.s)) return val_error(\"Path traversal detected\"); remove(path.data.s); return val_int(1); }\n\n");
    fprintf(out, "Value nr_rt_now() { return val_int((int)time(NULL)); }\nValue nr_rt_sleep(Value ms) { if (ms.type == VAL_INT) usleep(ms.data.i * 1000); return val_nil(); }\nValue nr_rt_random() { return val_int(rand()); }\n\n");
    fprintf(out, "Value nr_rt_substring(Value s, Value start, Value len) { if (s.type != VAL_STR || start.type != VAL_INT || len.type != VAL_INT) return val_nil(); int slen = strlen(s.data.s); int istart = start.data.i; int ilen = len.data.i; if (istart < 0) istart = 0; if (istart >= slen) return val_str(\"\"); if (ilen < 0) ilen = 0; if (istart + ilen > slen) ilen = slen - istart; char* sub = nr_alloc(ilen + 1); strncpy(sub, s.data.s + istart, ilen); sub[ilen] = 0; return val_str(sub); }\n\n");
    fprintf(out, "Value nr_rt_exec(Value cmd) { if (cmd.type != VAL_STR) return val_int(-1); for(char* p=cmd.data.s; *p; p++) { if(!isalnum(*p) && *p!='.' && *p!='/' && *p!='_' && *p!='-' && *p!=' ') return val_error(\"Illegal character in command\"); } return val_int(system(cmd.data.s)); }\n");
    fprintf(out, "Value nr_rt_str_index_of(Value s, Value sub) { if (s.type != VAL_STR || sub.type != VAL_STR) return val_int(-1); char* p = strstr(s.data.s, sub.data.s); if (!p) return val_int(-1); int res = (int)(p - s.data.s); return val_int(res); }\n");
    fprintf(out, "Value nr_rt_str_match(Value s, Value regex_str) { if (s.type != VAL_STR || regex_str.type != VAL_STR) return val_bool(0); regex_t regex; int reti = regcomp(&regex, regex_str.data.s, REG_EXTENDED); if (reti) return val_bool(0); reti = regexec(&regex, s.data.s, 0, NULL, 0); regfree(&regex); return val_bool(!reti); }\n");
    fprintf(out, "Value nr_rt_args() { Value a = val_arr(); for(int i=0; i<nr_argc; i++) nr_rt_push(a, val_str(nr_argv[i])); return a; }\n");
    
    fprintf(out, "char* val_to_json(Value v) { if (v.type == VAL_INT) { char* b = nr_alloc(32); sprintf(b, \"%%d\", v.data.i); return b; } if (v.type == VAL_STR) { char* b = nr_alloc(strlen(v.data.s) + 3); sprintf(b, \"\\\"%%s\\\"\", v.data.s); return b; } if (v.type == VAL_BOOL) return nr_strdup(v.data.i ? \"true\" : \"false\"); if (v.type == VAL_ARR) { char* res = nr_strdup(\"[\"); for (int i=0; i<v.data.arr->count; i++) { char* item = val_to_json(*v.data.arr->elements[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(item) + 3); sprintf(res, \"%%s%%s%%s\", old, item, i < v.data.arr->count-1 ? \",\" : \"\"); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, \"%%s]\", old); return res; } if (v.type == VAL_OBJ) { char* res = nr_strdup(\"{\"); for (int i=0; i<v.data.obj->count; i++) { char* val = val_to_json(*v.data.obj->values[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(v.data.obj->keys[i]) + strlen(val) + 6); sprintf(res, \"%%s\\\"%%s\\\":%%s%%s\", old, v.data.obj->keys[i], val, i < v.data.obj->count-1 ? \",\" : \"\"); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, \"%%s}\", old); return res; } return nr_strdup(\"null\"); }\n");
    fprintf(out, "Value nr_rt_json_encode(Value v) { return val_str(val_to_json(v)); }\n\n");
    
    fprintf(out, "Value nr_rt_db_open(Value path) { return path; }\n");
    fprintf(out, "void bind_val(sqlite3_stmt* stmt, int idx, Value v) {\n");
    fprintf(out, "  if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, idx, v.data.i);\n");
    fprintf(out, "  else if (v.type == VAL_STR) sqlite3_bind_text(stmt, idx, v.data.s, -1, SQLITE_TRANSIENT);\n");
    fprintf(out, "  else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, idx);\n");
    fprintf(out, "}\n");
    fprintf(out, "Value nr_rt_db_exec(Value db, Value sql, Value params) {\n");
    fprintf(out, "  if (sql.type != VAL_STR || db.type != VAL_STR) return val_int(0);\n");
    fprintf(out, "  sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return val_int(0);\n");
    fprintf(out, "  sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {\n");
    fprintf(out, "    if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }\n");
    fprintf(out, "    sqlite3_step(stmt); sqlite3_finalize(stmt);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  sqlite3_close(h); return val_int(1);\n");
    fprintf(out, "}\n");
    fprintf(out, "Value nr_rt_db_query(Value db, Value sql, Value params) {\n");
    fprintf(out, "  Value a = val_arr(); if (sql.type != VAL_STR || db.type != VAL_STR) return a;\n");
    fprintf(out, "  sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return a;\n");
    fprintf(out, "  sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {\n");
    fprintf(out, "    if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }\n");
    fprintf(out, "    int cols = sqlite3_column_count(stmt);\n");
    fprintf(out, "    while (sqlite3_step(stmt) == SQLITE_ROW) {\n");
    fprintf(out, "      Value o = val_obj();\n");
    fprintf(out, "      for (int i=0; i<cols; i++) {\n");
    fprintf(out, "        const char* name = sqlite3_column_name(stmt, i); int type = sqlite3_column_type(stmt, i);\n");
    fprintf(out, "        Value v = val_nil();\n");
    fprintf(out, "        if (type == SQLITE_INTEGER) v = val_int(sqlite3_column_int(stmt, i));\n");
    fprintf(out, "        else if (type == SQLITE_NULL) v = val_nil();\n");
    fprintf(out, "        else v = val_str((char*)sqlite3_column_text(stmt, i));\n");
    fprintf(out, "        set_field(o, name, v);\n");
    fprintf(out, "      }\n");
    fprintf(out, "      nr_rt_push(a, o);\n");
    fprintf(out, "    }\n");
    fprintf(out, "    sqlite3_finalize(stmt);\n");
    fprintf(out, "  }\n");
    fprintf(out, "  sqlite3_close(h); return a;\n");
    fprintf(out, "}\n");
    fprintf(out, "Value nr_rt_json_decode(Value s) {\n");
    fprintf(out, "  if (s.type != VAL_STR) return val_nil();\n");
    fprintf(out, "  char* p = s.data.s; while(*p == ' ' || *p == '[' || *p == ']') p++;\n");
    fprintf(out, "  if (*p == '{') {\n");
    fprintf(out, "    Value obj = val_obj(); p++;\n");
    fprintf(out, "    while(*p && *p != '}') {\n");
    fprintf(out, "      while(*p == ' ' || *p == '\"' || *p == ',') p++;\n");
    fprintf(out, "      char key[64]; int i=0; while(*p && *p != '\"') key[i++] = *p++; key[i] = 0; p++;\n");
    fprintf(out, "      while(*p == ' ' || *p == ':') p++;\n");
    fprintf(out, "      if (*p == '\"') { p++; char val[256]; int j=0; while(*p && *p != '\"') val[j++] = *p++; val[j] = 0; p++; set_field(obj, key, val_str(val)); }\n");
    fprintf(out, "      else { char val[32]; int j=0; while(*p && *p != ',' && *p != ' ' && *p != '}') val[j++] = *p++; val[j] = 0; set_field(obj, key, val_int(atoi(val))); }\n");
    fprintf(out, "    } return obj;\n");
    fprintf(out, "  } return val_nil();\n");
    fprintf(out, "}\n");
    fprintf(out, "Value nr_rt_http_serve(Value port, Value callback) {\n");
    fprintf(out, "  #ifdef _WIN32\n  WSADATA wsaData; WSAStartup(MAKEWORD(2,2), &wsaData);\n  #endif\n");
    fprintf(out, "  int server_fd, new_socket; struct sockaddr_in address; int addrlen = sizeof(address);\n");
    fprintf(out, "  server_fd = socket(AF_INET, SOCK_STREAM, 0);\n");
    fprintf(out, "  int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));\n");
    fprintf(out, "  address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(port.data.i);\n");
    fprintf(out, "  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror(\"bind\"); close(server_fd); return val_nil(); }\n");
    fprintf(out, "  if (listen(server_fd, 3) < 0) { perror(\"listen\"); close(server_fd); return val_nil(); }\n");
    fprintf(out, "  printf(\"HTTP Server listening on port %%d...\\n\", port.data.i);\n");
    fprintf(out, "  while(1) {\n    void* cp = nr_checkpoint();\n    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);\n");
    fprintf(out, "    char buffer[30000] = {0}; int n = recv(new_socket, buffer, 30000, 0);\n");
    fprintf(out, "    if (n <= 0) { close(new_socket); nr_rollback(cp); continue; }\n");
    fprintf(out, "    char method[16]={0}, path[1024]={0};\n");
    fprintf(out, "    if(sscanf(buffer, \"%%15s %%1023s\", method, path) < 2) { close(new_socket); nr_rollback(cp); continue; }\n");
    fprintf(out, "    Value req = val_obj(); set_field(req, \"method\", val_str(method)); set_field(req, \"path\", val_str(path));\n");
    fprintf(out, "    char* body_ptr = strstr(buffer, \"\\r\\n\\r\\n\"); if (body_ptr) { body_ptr += 4; set_field(req, \"body\", val_str(body_ptr)); } else { set_field(req, \"body\", val_nil()); }\n");
    fprintf(out, "    Value res = ((Value (*)(Value, Value, Value, Value, Value, Value))callback.data.func_ptr)(val_nil(), req, val_nil(), val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "    char* res_body = \"Not Found\"; int status = 404;\n");
    fprintf(out, "    if (res.type == VAL_OBJ) {\n");
    fprintf(out, "      Value b = get_field(res, \"body\"); if (b.type == VAL_STR) res_body = b.data.s;\n");
    fprintf(out, "      Value s = get_field(res, \"status\"); if (s.type == VAL_INT) status = s.data.i;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    char response[32000];\n");
    fprintf(out, "    snprintf(response, sizeof(response), \"HTTP/1.1 %%d OK\\r\\nContent-Type: application/json\\r\\nContent-Length: %%ld\\r\\n\\r\\n%%s\", status, strlen(res_body), res_body);\n");
    fprintf(out, "    send(new_socket, response, strlen(response), 0); close(new_socket);\n");
    fprintf(out, "    nr_rollback(cp);\n");
    fprintf(out, "  } return val_nil();\n");
    fprintf(out, "}\n");
}

static void collect_functions(AstNode* node, FILE* out) {
    if (!node) return;
    switch(node->type) {
        case AST_PROGRAM: for (int i=0; i<node->data.program.count; i++) collect_functions(node->data.program.statements[i], out); break;
        case AST_FUNC_DECL: {
            if (strcmp(node->data.func_decl.name, "anonymous") == 0) {
                char buf[32]; sprintf(buf, "lambda_%d", ++global_lambda_count);
                free(node->data.func_decl.name); node->data.func_decl.name = strdup(buf);
            } else if (current_mod) {
                char buf[256]; sprintf(buf, "%s_%s", current_mod, node->data.func_decl.name);
                free(node->data.func_decl.name); node->data.func_decl.name = strdup(buf);
            }
            if (function_count < 8192) function_names[function_count++] = strdup(node->data.func_decl.name);
            if (strcmp(node->data.func_decl.name, "main") != 0) {
                fprintf(out, "Value nr_%s(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);\n", node->data.func_decl.name);
            }
            collect_functions(node->data.func_decl.body, out);
            break;
        }
        case AST_IMPORT: {
            const char* m = node->data.import_stmt.path;
            for (int i=0; i<collected_module_count; i++) {
                if (strcmp(collected_modules[i], m) == 0) {
                    node->data.import_stmt.module_prog = module_progs[i];
                    return;
                }
            }
            int idx = collected_module_count++;
            collected_modules[idx] = strdup(m);
            fprintf(out, "void nr_%s_init();\n", m);
            char path[512]; char* src = NULL;
            for (int i=0; i<include_path_count; i++) { sprintf(path, "%s/%s.nr", include_paths[i], m); src = nr_read_file(path); if (src) break; }
            if (!src) { sprintf(path, "lib/%s.nr", m); src = nr_read_file(path); }
            if (!src) { sprintf(path, "%s.nr", m); src = nr_read_file(path); }
            if (src) {
                char* old = current_mod; current_mod = (char*)m;
                Lexer l; lexer_init(&l, src); Parser p; parser_init(&p, &l); AstNode* prog = parse_program(&p);
                module_progs[idx] = prog;
                collect_functions(prog, out); node->data.import_stmt.module_prog = prog; current_mod = old;
            }
            break;
        }
        case AST_IF: collect_functions(node->data.if_stmt.condition, out); collect_functions(node->data.if_stmt.then_branch, out); if (node->data.if_stmt.else_branch) collect_functions(node->data.if_stmt.else_branch, out); break;
        case AST_WHILE: collect_functions(node->data.while_stmt.condition, out); collect_functions(node->data.while_stmt.body, out); break;
        case AST_FOR: collect_functions(node->data.for_stmt.iterable, out); collect_functions(node->data.for_stmt.body, out); break;
        case AST_ASSIGN: collect_functions(node->data.assign.value, out); break;
        case AST_RETURN: collect_functions(node->data.ret.value, out); break;
        case AST_CALL: for (int i=0; i<node->data.call.arg_count; i++) collect_functions(node->data.call.args[i], out); break;
        case AST_BINARY: collect_functions(node->data.binary.left, out); collect_functions(node->data.binary.right, out); break;
        case AST_ARRAY: for (int i=0; i<node->data.array.count; i++) collect_functions(node->data.array.elements[i], out); break;
        case AST_OBJECT: for (AstField* f = node->data.object.fields; f; f = f->next) collect_functions(f->value, out); break;
        case AST_DESTRUCTURING: collect_functions(node->data.destruct.value, out); break;
        default: break;
    }
}

static void collect_vars(AstNode* node, const char** declared, int* count) {
    if (!node) return;
    if (node->type == AST_ASSIGN && !strchr(node->data.assign.target, '.')) {
        for (int i=0; i<*count; i++) if (strcmp(declared[i], node->data.assign.target) == 0) goto next;
        if (*count < 1024) declared[(*count)++] = strdup(node->data.assign.target);
    } next:;
    switch(node->type) {
        case AST_PROGRAM: for (int i=0; i<node->data.program.count; i++) collect_vars(node->data.program.statements[i], declared, count); break;
        case AST_IF: collect_vars(node->data.if_stmt.then_branch, declared, count); if (node->data.if_stmt.else_branch) collect_vars(node->data.if_stmt.else_branch, declared, count); break;
        case AST_FOR: {
            if (*count < 1024) declared[(*count)++] = strdup(node->data.for_stmt.var);
            if (node->data.for_stmt.alias && *count < 1024) declared[(*count)++] = strdup(node->data.for_stmt.alias);
            collect_vars(node->data.for_stmt.body, declared, count); break;
        }
        case AST_WHILE: collect_vars(node->data.while_stmt.body, declared, count); break;
        case AST_IMPORT: {
            const char* target = node->data.import_stmt.alias ? node->data.import_stmt.alias : node->data.import_stmt.path;
            if (*count < 1024) declared[(*count)++] = strdup(target);
            for (int i=0; i<node->data.import_stmt.symbol_count; i++) if (*count < 1024) declared[(*count)++] = strdup(node->data.import_stmt.symbols[i]);
            break;
        }
        case AST_DESTRUCTURING: {
            AstNode* t = node->data.destruct.target;
            if (t->type == AST_OBJECT) {
                for (AstField* f = t->data.object.fields; f; f = f->next) if (*count < 1024) declared[(*count)++] = strdup(f->alias ? f->alias : f->name);
            }
            break;
        }
        case AST_FUNC_DECL: {
            if (node->data.func_decl.name && strcmp(node->data.func_decl.name, "main") != 0 && strcmp(node->data.func_decl.name, "anonymous") != 0) {
                if (*count < 1024) declared[(*count)++] = strdup(node->data.func_decl.name);
            }
            break;
        }
        default: break;
    }
}

static void generate_functions(AstNode* node, AstNode* mod, FILE* out);

static void generate_functions(AstNode* node, AstNode* mod, FILE* out) {
    if (!node) return;
    switch(node->type) {
        case AST_PROGRAM: for (int i=0; i<node->data.program.count; i++) generate_functions(node->data.program.statements[i], node, out); break;
        case AST_IMPORT: {
            const char* m = node->data.import_stmt.path;
            for (int i=0; i<generated_module_count; i++) if (strcmp(generated_modules[i], m) == 0) return;
            if (generated_module_count < 128) generated_modules[generated_module_count++] = strdup(m);
            if (node->data.import_stmt.module_prog) {
                generate_functions(node->data.import_stmt.module_prog, node->data.import_stmt.module_prog, out);
                fprintf(out, "void nr_%s_init() { static bool init = false; if (init) return; init = true;\n", m);
                fprintf(out, "  if (nr_v_%s.type == VAL_NIL) nr_v_%s = val_obj();\n", m, m);
                for (int i=0; i<node->data.import_stmt.module_prog->data.program.count; i++) {
                    AstNode* s = node->data.import_stmt.module_prog->data.program.statements[i];
                    if (s->type == AST_FUNC_DECL) {
                        const char* n = s->data.func_decl.name;
                        if (strcmp(n, "main") != 0 && strcmp(n, "anonymous") != 0) {
                            const char* sn = strrchr(n, '_'); if (sn) sn++; else sn = n;
                            fprintf(out, "  set_field(nr_v_%s, \"%s\", val_func(nr_%s));\n", m, sn, n);
                        }
                    }
                }
                char* old_pref = current_pref; current_pref = (char*)m;
                codegen_c_node(node->data.import_stmt.module_prog, out);
                current_pref = old_pref;
                fprintf(out, "}\n\n");
            }
            break;
        }
        case AST_FUNC_DECL: {
            if (strcmp(node->data.func_decl.name, "main") == 0) break;
            fprintf(out, "Value nr_%s(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {\n", node->data.func_decl.name);
            const char* vars[1024]; int vcount = 0; vars[vcount++] = "self";
            for (int i=0; i<node->data.func_decl.param_count; i++) {
                fprintf(out, "  Value nr_v_%s = nr_v_a%d;\n", node->data.func_decl.params[i], i+1);
                vars[vcount++] = node->data.func_decl.params[i];
            }
            
            char* name = node->data.func_decl.name; char* last_und = strrchr(name, '_');
            char* old_pref = current_pref;
            if (strncmp(name, "lambda", 6) != 0) {
                if (last_und) { char m_name[64]; int len = last_und - name; strncpy(m_name, name, len); m_name[len] = 0; current_pref = strdup(m_name); }
                else current_pref = NULL;
            }
            
            collect_vars(node->data.func_decl.body, vars, &vcount);
            for (int i=0; i<vcount; i++) {
                int skip = 0; if (strcmp(vars[i], "self") == 0) skip = 1;
                for (int j=0; j<node->data.func_decl.param_count; j++) if (strcmp(vars[i], node->data.func_decl.params[j]) == 0) skip = 1;
                if (is_global(vars[i])) skip = 1;
                if (!skip) { 
                    int dup = 0; for (int k=0; k<i; k++) if (strcmp(vars[k], vars[i]) == 0) { dup = 1; break; }
                    if (!dup) fprintf(out, "  Value nr_v_%s = val_nil(); (void)nr_v_%s;\n", vars[i], vars[i]);
                }
            }
            if (node->data.func_decl.body->type != AST_PROGRAM) { fprintf(out, "  return "); codegen_c_node(node->data.func_decl.body, out); fprintf(out, ";"); }
            else codegen_c_node(node->data.func_decl.body, out);
            fprintf(out, "\n  return val_nil(); }\n\n");
            current_pref = old_pref;
            generate_functions(node->data.func_decl.body, mod, out);
            break;
        }
        case AST_IF: generate_functions(node->data.if_stmt.then_branch, mod, out); if (node->data.if_stmt.else_branch) generate_functions(node->data.if_stmt.else_branch, mod, out); break;
        case AST_WHILE: generate_functions(node->data.while_stmt.body, mod, out); break;
        case AST_FOR: generate_functions(node->data.for_stmt.body, mod, out); break;
        case AST_ASSIGN: generate_functions(node->data.assign.value, mod, out); break;
        case AST_CALL: for (int i=0; i<node->data.call.arg_count; i++) generate_functions(node->data.call.args[i], mod, out); break;
        case AST_ARRAY: for (int i=0; i<node->data.array.count; i++) generate_functions(node->data.array.elements[i], mod, out); break;
        case AST_OBJECT: for (AstField* f = node->data.object.fields; f; f = f->next) generate_functions(f->value, mod, out); break;
        case AST_DESTRUCTURING: generate_functions(node->data.destruct.value, mod, out); break;
        case AST_RETURN: generate_functions(node->data.ret.value, mod, out); break;
        case AST_BINARY: generate_functions(node->data.binary.left, mod, out); generate_functions(node->data.binary.right, mod, out); break;
        default: break;
    }
}

void codegen_c_node(AstNode* node, FILE* out) {
    if (!node) return;
    switch(node->type) {
        case AST_PROGRAM: for (int i=0; i<node->data.program.count; i++) { codegen_c_node(node->data.program.statements[i], out); fprintf(out, ";\n"); } break;
        case AST_RETURN: fprintf(out, "  return "); codegen_c_node(node->data.ret.value, out); break;
        case AST_IF: fprintf(out, "  if (is_truthy("); codegen_c_node(node->data.if_stmt.condition, out); fprintf(out, ")) {\n"); codegen_c_node(node->data.if_stmt.then_branch, out); fprintf(out, "  }"); if (node->data.if_stmt.else_branch) { fprintf(out, " else {\n"); codegen_c_node(node->data.if_stmt.else_branch, out); fprintf(out, "  }\n"); } break;
        case AST_WHILE: fprintf(out, "  while (is_truthy("); codegen_c_node(node->data.while_stmt.condition, out); fprintf(out, ")) {\n"); codegen_c_node(node->data.while_stmt.body, out); fprintf(out, "  }\n"); break;
        case AST_FOR: {
            fprintf(out, "  if ("); codegen_c_node(node->data.for_stmt.iterable, out); fprintf(out, ".type == VAL_ARR) {\n");
            char buf[32]; sprintf(buf, "_i%d", ++global_lambda_count);
            fprintf(out, "  for (int %s = 0; %s < ", buf, buf);
            codegen_c_node(node->data.for_stmt.iterable, out);
            fprintf(out, ".data.arr->count; %s++) {\n", buf);
            fprintf(out, "    nr_v_%s = *", node->data.for_stmt.var);
            codegen_c_node(node->data.for_stmt.iterable, out);
            fprintf(out, ".data.arr->elements[%s];\n", buf);
            if (node->data.for_stmt.alias) fprintf(out, "    nr_v_%s = nr_v_%s;\n", node->data.for_stmt.alias, node->data.for_stmt.var);
            codegen_c_node(node->data.for_stmt.body, out);
            fprintf(out, "  }\n  }\n");
            break;
        }
        case AST_ASSIGN: if (strchr(node->data.assign.target, '.')) { char* t = strdup(node->data.assign.target); char* d = strchr(t, '.'); *d = 0; fprintf(out, "  set_field(nr_v_%s, \"%s\", ", t, d + 1); codegen_c_node(node->data.assign.value, out); fprintf(out, ");\n"); free(t); } else { fprintf(out, "  nr_v_%s = ", node->data.assign.target); codegen_c_node(node->data.assign.value, out); } break;
        case AST_DESTRUCTURING: { fprintf(out, "({ Value _tmp = "); codegen_c_node(node->data.destruct.value, out); fprintf(out, "; "); AstField* f = node->data.destruct.target->data.object.fields; while (f) { const char* n = f->alias ? f->alias : f->name; fprintf(out, "nr_v_%s = get_field(_tmp, \"%s\"); ", n, f->name); f = f->next; } fprintf(out, "_tmp; })"); break; }
        case AST_VAR_REF:
            if (strcmp(node->data.var_name, "null") == 0) fprintf(out, "val_nil()");
            else if (strcmp(node->data.var_name, "true") == 0) fprintf(out, "val_bool(true)");
            else if (strcmp(node->data.var_name, "false") == 0) fprintf(out, "val_bool(false)");
            else if (strcmp(node->data.var_name, "self") == 0) fprintf(out, "self");
            else if (strchr(node->data.var_name, '.')) {
                char* n = strdup(node->data.var_name);
                char* dot = strchr(n, '.');
                *dot = '\0';
                
                char* remaining = dot + 1;
                char* next_dot = strchr(remaining, '.');
                
                if (!next_dot) {
                    fprintf(out, "get_field(nr_v_%s, \"%s\")", n, remaining);
                } else {
                    char* parts[16];
                    int part_count = 0;
                    parts[part_count++] = n;
                    char* curr = remaining;
                    while ((next_dot = strchr(curr, '.'))) {
                        *next_dot = '\0';
                        parts[part_count++] = curr;
                        curr = next_dot + 1;
                    }
                    parts[part_count++] = curr;
                    
                    for (int i=0; i<part_count-1; i++) fprintf(out, "get_field(");
                    fprintf(out, "nr_v_%s", parts[0]);
                    for (int i=1; i<part_count; i++) fprintf(out, ", \"%s\")", parts[i]);
                }
                free(n);
            }
            else if (is_function(node->data.var_name)) fprintf(out, "val_func(nr_%s)", node->data.var_name);
            else { char p[256]; if (current_pref) { sprintf(p, "%s_%s", current_pref, node->data.var_name); if (is_function(p)) { fprintf(out, "val_func(nr_%s)", p); goto end; } else if (is_global(p)) { fprintf(out, "nr_v_%s", p); goto end; } } fprintf(out, "nr_v_%s", node->data.var_name); }
            end: break;
        case AST_LITERAL_INT: fprintf(out, "val_int(%d)", node->data.int_val); break;
        case AST_LITERAL_FLOAT: fprintf(out, "val_float(%g)", node->data.float_val); break;
        case AST_LITERAL_STR: fprintf(out, "val_str(\"%s\")", node->data.str_val); break;
        case AST_LITERAL_BOOL: fprintf(out, "val_bool(%d)", node->data.int_val); break;
        case AST_LITERAL_NULL: fprintf(out, "val_nil()"); break;
        case AST_OBJECT: fprintf(out, "({ Value _o = val_obj(); "); for (AstField* f = node->data.object.fields; f; f = f->next) { fprintf(out, "set_field(_o, \"%s\", ", f->name); codegen_c_node(f->value, out); fprintf(out, "); "); } fprintf(out, "_o; })"); break;
        case AST_ARRAY: fprintf(out, "({ Value _a = val_arr(); "); for (int i=0; i<node->data.array.count; i++) { fprintf(out, "nr_rt_push(_a, "); codegen_c_node(node->data.array.elements[i], out); fprintf(out, "); "); } fprintf(out, "_a; })"); break;
        case AST_INDEX: fprintf(out, "nr_rt_at("); codegen_c_node(node->data.index.object, out); fprintf(out, ", "); codegen_c_node(node->data.index.index, out); fprintf(out, ")"); break;
        case AST_INDEX_ASSIGN: fprintf(out, "nr_rt_set_at("); codegen_c_node(node->data.index_assign.object, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.index, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.value, out); fprintf(out, ")"); break;
        case AST_IMPORT: {
            const char* m = node->data.import_stmt.path;
            const char* alias = node->data.import_stmt.alias;
            const char* mod_var = alias ? alias : m;
            fprintf(out, "  nr_%s_init();\n", m);
            fprintf(out, "  if (nr_v_%s.type == VAL_NIL) nr_v_%s = val_obj();\n", mod_var, mod_var);
            AstNode* p = node->data.import_stmt.module_prog;
            if (p) {
                for (int i=0; i<p->data.program.count; i++) {
                    AstNode* s = p->data.program.statements[i];
                    if (s->type == AST_FUNC_DECL) {
                        const char* n = s->data.func_decl.name;
                        if (strcmp(n, "main") != 0 && strcmp(n, "anonymous") != 0) {
                            const char* sn = strrchr(n, '_'); if (sn) sn++; else sn = n;
                            fprintf(out, "  set_field(nr_v_%s, \"%s\", val_func(nr_%s));\n", mod_var, sn, n);
                        }
                    }
                }
            }
            for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
                const char* sym = node->data.import_stmt.symbols[i];
                fprintf(out, "  nr_v_%s = get_field(nr_v_%s, \"%s\");\n", sym, mod_var, sym);
            }
            break;
        }
        case AST_CALL: {
            const char* n = node->data.call.name;
            if (strcmp(n, "print") == 0) { fprintf(out, "nr_rt_print("); for (int i=0; i<node->data.call.arg_count; i++) { codegen_c_node(node->data.call.args[i], out); if (i < node->data.call.arg_count - 1) fprintf(out, ", "); } fprintf(out, ")"); }
            else if (strcmp(n, "len") == 0 || strcmp(n, "__builtin_len") == 0) { fprintf(out, "nr_rt_len("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_push") == 0) { fprintf(out, "nr_rt_push("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); if (node->data.call.arg_count > 1) { fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); } fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_pop") == 0) { fprintf(out, "nr_rt_pop("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_file_read") == 0) { fprintf(out, "nr_rt_file_read("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_file_write") == 0) { fprintf(out, "nr_rt_file_write("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); if (node->data.call.arg_count > 1) { fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); } fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_file_exists") == 0) { fprintf(out, "nr_rt_file_exists("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_file_delete") == 0) { fprintf(out, "nr_rt_file_delete("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_now") == 0 || strcmp(n, "__builtin_time_now") == 0) fprintf(out, "nr_rt_now()");
            else if (strcmp(n, "__builtin_sleep") == 0 || strcmp(n, "__builtin_delay") == 0) { fprintf(out, "nr_rt_sleep("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_random") == 0) fprintf(out, "nr_rt_random()");
            else if (strcmp(n, "__builtin_substring") == 0) { fprintf(out, "nr_rt_substring("); for (int i=0; i<node->data.call.arg_count; i++) { codegen_c_node(node->data.call.args[i], out); if (i < node->data.call.arg_count - 1) fprintf(out, ", "); } fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_exec") == 0) { fprintf(out, "nr_rt_exec("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_exit_proc") == 0) { fprintf(out, "nr_rt_exit("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "toInt") == 0) { fprintf(out, "nr_rt_to_int("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_args") == 0) { fprintf(out, "nr_rt_args()"); }
            else if (strcmp(n, "__builtin_http_serve") == 0) { fprintf(out, "nr_rt_http_serve("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_db_open") == 0) { fprintf(out, "nr_rt_db_open("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_db_exec") == 0) { fprintf(out, "nr_rt_db_exec("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[2], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_db_query") == 0) { fprintf(out, "nr_rt_db_query("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[2], out); fprintf(out, ")"); }
            else if (strcmp(n, "json_decode") == 0) { fprintf(out, "nr_rt_json_decode("); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_json_encode") == 0) { fprintf(out, "nr_rt_json_encode("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_str_index_of") == 0) { fprintf(out, "nr_rt_str_index_of("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_str_replace") == 0) { fprintf(out, "nr_rt_str_replace("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[2], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_str_match") == 0) { fprintf(out, "nr_rt_str_match("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ")"); }
            else if (strcmp(n, "__builtin_obj_keys") == 0) { fprintf(out, "nr_rt_obj_keys("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ")"); }
            else if (strchr(n, '.')) {

                char* t = strdup(n); char* d = strchr(t, '.'); *d = 0; const char* method = d + 1;
                fprintf(out, "({ Value _t = nr_v_%s; Value _f = get_field(_t, \"%s\"); Value _r; ", t, method);
                fprintf(out, "if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t");
                for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
                for (int i=node->data.call.arg_count; i<5; i++) fprintf(out, ", val_nil()");
                fprintf(out, "); else { ");
                if (strcmp(method, "substring") == 0) {
                    if (node->data.call.arg_count == 3) { fprintf(out, "_r = nr_rt_substring("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[2], out); fprintf(out, "); "); }
                    else { fprintf(out, "_r = nr_rt_substring(_t"); for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); } fprintf(out, "); "); }
                }
                else if (strcmp(method, "length") == 0) { fprintf(out, "_r = nr_rt_len(_t); "); }
                else if (strcmp(method, "push") == 0) { fprintf(out, "_r = nr_rt_push(_t, "); if (node->data.call.arg_count > 0) codegen_c_node(node->data.call.args[0], out); else fprintf(out, "val_nil()"); fprintf(out, "); "); }
                else if (strcmp(method, "pop") == 0) { fprintf(out, "_r = nr_rt_pop(_t); "); }
                else {
                    char p_buf[256]; sprintf(p_buf, "%s_%s", t, method);
                    if (is_function(p_buf)) { 
                        fprintf(out, "_r = nr_%s(_t", p_buf); 
                        for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); } 
                        for (int i=node->data.call.arg_count; i<5; i++) fprintf(out, ", val_nil()");
                        fprintf(out, "); "); 
                    }
                    else if (is_function(method)) { 
                        fprintf(out, "_r = nr_%s(val_nil(), _t", method); 
                        for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); } 
                        for (int i=node->data.call.arg_count; i<4; i++) fprintf(out, ", val_nil()"); // 4 because _t is already passed
                        fprintf(out, "); "); 
                    }
                    else if (current_pref) {
                        char p2[256]; sprintf(p2, "%s_%s", current_pref, method);
                        if (is_function(p2)) { 
                            fprintf(out, "_r = nr_%s(val_nil(), _t", p2); 
                            for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); } 
                            for (int i=node->data.call.arg_count; i<4; i++) fprintf(out, ", val_nil()");
                            fprintf(out, "); "); 
                        }
                        else goto fallback;
                    } else {
                        fallback:
                        if (is_global(method)) { fprintf(out, "if (nr_v_%s.type == VAL_FUNC && nr_v_%s.data.func_ptr) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))nr_v_%s.data.func_ptr)(val_nil(), _t, val_nil(), val_nil(), val_nil(), val_nil()); else _r = val_nil(); ", method, method, method); }
                        else { fprintf(out, "_r = val_nil(); "); }
                    }
                }
                fprintf(out, "} _r; })"); free(t);
            } else {
                char p_buf[256]; if (current_pref) { sprintf(p_buf, "%s_%s", current_pref, n); if (is_function(p_buf)) { fprintf(out, "nr_%s(val_nil()", p_buf); goto args; } }
                if (is_function(n)) { fprintf(out, "nr_%s(val_nil()", n); }
                else { fprintf(out, "(void)({ Value _f = "); 
                    if (current_pref) { char p[256]; sprintf(p, "%s_%s", current_pref, n); if (is_global(p)) fprintf(out, "nr_v_%s", p); else if (is_function(p)) fprintf(out, "val_func(nr_%s)", p); else fprintf(out, "nr_v_%s", n); }
                    else fprintf(out, "nr_v_%s", n);
                    fprintf(out, "; Value _r; if (_f.type == VAL_FUNC && _f.data.func_ptr) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil()"); }
                args: for (int i=0; i<node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); } 
                int total_args = node->data.call.arg_count + 1; // +1 for self
                if (is_function(n) || (current_pref && is_function(p_buf))) {
                  // already printed val_nil() as self
                } else {
                  // already printed val_nil() as self
                }
                for (int i=total_args; i<6; i++) fprintf(out, ", val_nil()");
                if (!is_function(n) && (!current_pref || !is_function(p_buf))) fprintf(out, "); else { nr_rt_print(val_error(\"Function not found: %s\")); _r = val_nil(); } _r; })", n); else fprintf(out, ")");
            }
            break;
        }
        case AST_BINARY: {
            if (strcmp(node->data.binary.op, "==") == 0) {
                fprintf(out, "nr_rt_eq(");
                codegen_c_node(node->data.binary.left, out);
                fprintf(out, ", ");
                codegen_c_node(node->data.binary.right, out);
                fprintf(out, ")");
            } else if (strcmp(node->data.binary.op, "!=") == 0) {
                fprintf(out, "val_bool(!is_truthy(nr_rt_eq(");
                codegen_c_node(node->data.binary.left, out);
                fprintf(out, ", ");
                codegen_c_node(node->data.binary.right, out);
                fprintf(out, ")))");
            } else if (strcmp(node->data.binary.op, "+") == 0) { fprintf(out, "nr_rt_add("); codegen_c_node(node->data.binary.left, out); fprintf(out, ", "); codegen_c_node(node->data.binary.right, out); fprintf(out, ")"); }
            else if (strcmp(node->data.binary.op, "and") == 0) { fprintf(out, "({ Value _l = "); codegen_c_node(node->data.binary.left, out); fprintf(out, "; is_truthy(_l) ? "); codegen_c_node(node->data.binary.right, out); fprintf(out, " : _l; })"); }
            else if (strcmp(node->data.binary.op, "or") == 0) { fprintf(out, "({ Value _l = "); codegen_c_node(node->data.binary.left, out); fprintf(out, "; is_truthy(_l) ? _l : "); codegen_c_node(node->data.binary.right, out); fprintf(out, "; })"); }
            else if (strcmp(node->data.binary.op, "not") == 0) { fprintf(out, "val_bool(!is_truthy("); codegen_c_node(node->data.binary.left, out); fprintf(out, "))"); }
            else {
                fprintf(out, "val_int(");
                codegen_c_node(node->data.binary.left, out);
                fprintf(out, ".data.i %s ", node->data.binary.op);
                codegen_c_node(node->data.binary.right, out);
                fprintf(out, ".data.i)");
            }
            break;
        }
        case AST_FUNC_DECL: if (strcmp(node->data.func_decl.name, "main") != 0) fprintf(out, "val_func(nr_%s)", node->data.func_decl.name); break;
        case AST_ERROR: fprintf(out, "val_error("); codegen_c_node(node->data.error_expr.message, out); fprintf(out, ".data.s)"); break;
        case AST_BREAK: fprintf(out, "break"); break;
        case AST_CONTINUE: fprintf(out, "continue"); break;
        case AST_PASS: break;
        default: break;
    }
}

static void collect_all_globals(AstNode* node) {
    if (!node) return;
    switch(node->type) {
        case AST_PROGRAM:
            for (int i=0; i<node->data.program.count; i++) collect_all_globals(node->data.program.statements[i]);
            break;
        case AST_ASSIGN:
            if (!strchr(node->data.assign.target, '.')) {
                if (!is_global(node->data.assign.target)) global_vars[global_var_count++] = strdup(node->data.assign.target);
            }
            collect_all_globals(node->data.assign.value);
            break;
        case AST_FOR:
            if (!is_global(node->data.for_stmt.var)) global_vars[global_var_count++] = strdup(node->data.for_stmt.var);
            if (node->data.for_stmt.alias && !is_global(node->data.for_stmt.alias)) global_vars[global_var_count++] = strdup(node->data.for_stmt.alias);
            collect_all_globals(node->data.for_stmt.iterable);
            collect_all_globals(node->data.for_stmt.body);
            break;
        case AST_IMPORT: {
            const char* target = node->data.import_stmt.alias ? node->data.import_stmt.alias : node->data.import_stmt.path;
            if (!is_global(target)) global_vars[global_var_count++] = strdup(target);
            for (int j=0; j<node->data.import_stmt.symbol_count; j++) {
                if (!is_global(node->data.import_stmt.symbols[j])) global_vars[global_var_count++] = strdup(node->data.import_stmt.symbols[j]);
            }
            collect_all_globals(node->data.import_stmt.module_prog);
            break;
        }
        case AST_FUNC_DECL:
            if (node->data.func_decl.name && strcmp(node->data.func_decl.name, "main") != 0 && strcmp(node->data.func_decl.name, "anonymous") != 0) {
                if (!is_global(node->data.func_decl.name)) global_vars[global_var_count++] = strdup(node->data.func_decl.name);
            }
            for (int i=0; i<node->data.func_decl.param_count; i++) {
                if (!is_global(node->data.func_decl.params[i])) global_vars[global_var_count++] = strdup(node->data.func_decl.params[i]);
            }
            collect_all_globals(node->data.func_decl.body);
            break;
        case AST_IF:
            collect_all_globals(node->data.if_stmt.condition);
            collect_all_globals(node->data.if_stmt.then_branch);
            collect_all_globals(node->data.if_stmt.else_branch);
            break;
        case AST_WHILE:
            collect_all_globals(node->data.while_stmt.condition);
            collect_all_globals(node->data.while_stmt.body);
            break;
        case AST_CALL:
            for (int i=0; i<node->data.call.arg_count; i++) collect_all_globals(node->data.call.args[i]);
            break;
        case AST_BINARY:
            collect_all_globals(node->data.binary.left);
            collect_all_globals(node->data.binary.right);
            break;
        case AST_ARRAY:
            for (int i=0; i<node->data.array.count; i++) collect_all_globals(node->data.array.elements[i]);
            break;
        case AST_OBJECT: {
            AstField* f = node->data.object.fields;
            while (f) { collect_all_globals(f->value); f = f->next; }
            break;
        }
        case AST_RETURN: collect_all_globals(node->data.ret.value); break;
        default: break;
    }
}

void codegen_c_program(AstNode* node, FILE* out) {
    print_runtime(out); collect_functions(node, out);
    collect_all_globals(node);
    for (int i=0; i<global_var_count; i++) fprintf(out, "Value nr_v_%s;\n", global_vars[i]);
    fprintf(out, "\n"); generate_functions(node, node, out); fprintf(out, "\n");
    AstNode* m_node = NULL; for (int i=0; i<node->data.program.count; i++) if (node->data.program.statements[i]->type == AST_FUNC_DECL && strcmp(node->data.program.statements[i]->data.func_decl.name, "main") == 0) { m_node = node->data.program.statements[i]; break; }
    if (m_node) {
        fprintf(out, "int main(int argc, char** argv) {\n");
        fprintf(out, "  nr_arena = malloc(sizeof(Arena)); nr_arena->blocks = NULL;\n");
        fprintf(out, "  srand(time(NULL));\n  nr_argc = argc; nr_argv = argv;\n  Value self = val_nil(); (void)self;\n");
        for (int i=0; i<global_var_count; i++) fprintf(out, "  nr_v_%s = val_nil();\n", global_vars[i]);
        current_pref = NULL; 
        codegen_c_node(node, out); 
        codegen_c_node(m_node->data.func_decl.body, out);
        fprintf(out, "\n  ArenaBlock* curr = nr_arena->blocks; while(curr) { ArenaBlock* next = curr->next; free(curr->ptr); free(curr); curr = next; } free(nr_arena);\n");
        fprintf(out, "  return 0; \n}\n");
    }
}
