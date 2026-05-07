#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <regex.h>
#include <ctype.h>
#include <sqlite3.h>
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  #define close closesocket
  #define mkdir(p, m) _mkdir(p)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

int nr_argc; char** nr_argv;
typedef enum { VAL_NIL, VAL_INT, VAL_FLOAT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_ERROR } ValueType;
typedef struct ArenaBlock { void* ptr; struct ArenaBlock* next; } ArenaBlock;
typedef struct { ArenaBlock* blocks; } Arena; Arena* nr_arena;
void* nr_alloc(size_t sz) { void* p = malloc(sz); ArenaBlock* b = malloc(sizeof(ArenaBlock)); b->ptr = p; b->next = nr_arena->blocks; nr_arena->blocks = b; return p; }
void* nr_checkpoint() { return nr_arena->blocks; }
void nr_rollback(void* cp) { ArenaBlock* curr = nr_arena->blocks; while(curr && curr != cp) { ArenaBlock* next = curr->next; free(curr->ptr); free(curr); curr = next; } nr_arena->blocks = cp; }
void nr_arena_clear() { nr_rollback(NULL); }
char* nr_strdup(const char* s) { char* d = nr_alloc(strlen(s)+1); strcpy(d, s); return d; }
struct Value; typedef struct Value { ValueType type; union { int i; double f; char* s; struct { char** keys; struct Value** values; int count; int capacity; }* obj; struct { struct Value** elements; int count; int capacity; }* arr; void* func_ptr; } data; } Value;

Value val_nil() { return (Value){.type = VAL_NIL}; }
Value val_int(int i) { return (Value){.type = VAL_INT, .data.i = i}; }
Value val_float(double f) { return (Value){.type = VAL_FLOAT, .data.f = f}; }
Value val_bool(bool b) { return (Value){.type = VAL_BOOL, .data.i = b ? 1 : 0}; }
Value val_str(const char* s) { return (Value){.type = VAL_STR, .data.s = nr_strdup(s)}; }
Value val_error(const char* m) { return (Value){.type = VAL_ERROR, .data.s = nr_strdup(m)}; }
Value val_func(void* ptr) { return (Value){.type = VAL_FUNC, .data.func_ptr = ptr}; }
bool is_truthy(Value v) { if (v.type == VAL_NIL) return false; if (v.type == VAL_BOOL || v.type == VAL_INT) return v.data.i != 0; if (v.type == VAL_FLOAT) return v.data.f != 0.0; return true; }

Value val_obj() { Value v; v.type = VAL_OBJ; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 8; v.data.obj->keys = nr_alloc(sizeof(char*) * 8); v.data.obj->values = nr_alloc(sizeof(Value*) * 8); return v; }
Value val_arr() { Value v; v.type = VAL_ARR; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 8; v.data.arr->elements = nr_alloc(sizeof(Value*) * 8); return v; }

void set_field(Value obj, const char* key, Value val) { if (obj.type != VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) { if(strcmp(obj.data.obj->keys[i], key) == 0) { *obj.data.obj->values[i] = val; return; } } if(obj.data.obj->count >= obj.data.obj->capacity) { int old_cap = obj.data.obj->capacity; obj.data.obj->capacity *= 2; char** new_keys = nr_alloc(sizeof(char*) * obj.data.obj->capacity); Value** new_vals = nr_alloc(sizeof(Value*) * obj.data.obj->capacity); memcpy(new_keys, obj.data.obj->keys, sizeof(char*)*old_cap); memcpy(new_vals, obj.data.obj->values, sizeof(Value*)*old_cap); obj.data.obj->keys = new_keys; obj.data.obj->values = new_vals; } obj.data.obj->keys[obj.data.obj->count] = nr_strdup(key); obj.data.obj->values[obj.data.obj->count] = nr_alloc(sizeof(Value)); *obj.data.obj->values[obj.data.obj->count] = val; obj.data.obj->count++; }
Value get_field(Value obj, const char* key) { if (obj.type != VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) { if(strcmp(obj.data.obj->keys[i], key) == 0) return *obj.data.obj->values[i]; } return val_nil(); }

Value nr_rt_push(Value arr, Value val) { if (arr.type != VAL_ARR) return val_nil(); if (arr.data.arr->count >= arr.data.arr->capacity) { int old_cap = arr.data.arr->capacity; arr.data.arr->capacity *= 2; Value** new_el = nr_alloc(sizeof(Value*) * arr.data.arr->capacity); memcpy(new_el, arr.data.arr->elements, sizeof(Value*)*old_cap); arr.data.arr->elements = new_el; } arr.data.arr->elements[arr.data.arr->count] = nr_alloc(sizeof(Value)); *arr.data.arr->elements[arr.data.arr->count] = val; arr.data.arr->count++; return val; }
Value nr_rt_obj_keys(Value obj) { if (obj.type != VAL_OBJ) return val_arr(); Value a = val_arr(); for(int i=0; i<obj.data.obj->count; i++) nr_rt_push(a, val_str(obj.data.obj->keys[i])); return a; }
Value nr_rt_pop(Value arr) { if (arr.type != VAL_ARR || arr.data.arr->count == 0) return val_nil(); arr.data.arr->count--; return *arr.data.arr->elements[arr.data.arr->count]; }
Value nr_rt_at(Value v, Value idx) { if (v.type == VAL_ARR && idx.type == VAL_INT) { int i = idx.data.i; if (i < 0 || i >= v.data.arr->count) return val_nil(); return *v.data.arr->elements[i]; } if (v.type == VAL_OBJ && idx.type == VAL_STR) return get_field(v, idx.data.s); if (v.type == VAL_STR && idx.type == VAL_INT) { int i = idx.data.i; if (i < 0 || i >= (int)strlen(v.data.s)) return val_nil(); char s[2] = {v.data.s[i], 0}; return val_str(s); } return val_nil(); }
Value nr_rt_set_at(Value v, Value idx, Value val) { if (v.type == VAL_ARR && idx.type == VAL_INT) { int i = idx.data.i; if (i >= 0 && i < v.data.arr->count) *v.data.arr->elements[i] = val; } else if (v.type == VAL_OBJ && idx.type == VAL_STR) set_field(v, idx.data.s, val); return val; }
Value nr_rt_to_int(Value v) { if (v.type == VAL_INT) return v; if (v.type == VAL_STR) return val_int((int)strtol(v.data.s, NULL, 10)); return val_int(0); }
void nr_rt_print(Value v) { if (v.type == VAL_INT) printf("%d\n", v.data.i); else if (v.type == VAL_BOOL) printf("%s\n", v.data.i ? "true" : "false"); else if (v.type == VAL_STR) printf("%s\n", v.data.s); else if (v.type == VAL_ARR) printf("[Array]\n"); else if (v.type == VAL_OBJ) printf("[Object]\n"); else if (v.type == VAL_ERROR) printf("Error: %s\n", v.data.s); else printf("nil\n"); fflush(stdout); }
Value nr_rt_len(Value v) { if (v.type == VAL_ARR) return val_int(v.data.arr->count); if (v.type == VAL_STR) return val_int(strlen(v.data.s)); return val_int(0); }
Value nr_rt_add(Value l, Value r) {
  if (l.type == VAL_STR || r.type == VAL_STR) {
    char buf_l[64], buf_r[64]; char *sl, *sr;
    if (l.type == VAL_STR) sl = l.data.s; else if (l.type == VAL_INT) { snprintf(buf_l, 64, "%d", l.data.i); sl = buf_l; } else if (l.type == VAL_FLOAT) { snprintf(buf_l, 64, "%g", l.data.f); sl = buf_l; } else sl = "nil";
    if (r.type == VAL_STR) sr = r.data.s; else if (r.type == VAL_INT) { snprintf(buf_r, 64, "%d", r.data.i); sr = buf_r; } else if (r.type == VAL_FLOAT) { snprintf(buf_r, 64, "%g", r.data.f); sr = buf_r; } else sr = "nil";
    char* res = nr_alloc(strlen(sl) + strlen(sr) + 1); strcpy(res, sl); strcat(res, sr); return val_str(res);
  }
  if (l.type == VAL_FLOAT || r.type == VAL_FLOAT) {
    double lv = (l.type == VAL_FLOAT) ? l.data.f : (double)l.data.i;
    double rv = (r.type == VAL_FLOAT) ? r.data.f : (double)r.data.i;
    return val_float(lv + rv);
  }
  return val_int(l.data.i + r.data.i);
}
Value nr_rt_eq(Value l, Value r) {
  if (l.type == r.type) {
    if (l.type == VAL_NIL) return val_bool(1);
    if (l.type == VAL_STR) return val_bool(strcmp(l.data.s, r.data.s) == 0);
    if (l.type == VAL_INT || l.type == VAL_BOOL) return val_bool(l.data.i == r.data.i);
    if (l.type == VAL_FLOAT) return val_bool(l.data.f == r.data.f);
    return val_bool(l.data.obj == r.data.obj);
  }
  if ((l.type == VAL_INT || l.type == VAL_FLOAT) && (r.type == VAL_INT || r.type == VAL_FLOAT)) {
    double lv = (l.type == VAL_FLOAT) ? l.data.f : (double)l.data.i;
    double rv = (r.type == VAL_FLOAT) ? r.data.f : (double)r.data.i;
    return val_bool(lv == rv);
  }
  return val_bool(0);
}

static int is_safe_path(const char* path) { if (!path) return 0; if (strstr(path, "..")) return 0; return 1; }
Value nr_rt_read_file(Value path) { if (path.type != VAL_STR) return val_nil(); FILE* f = fopen(path.data.s, "rb"); if (!f) return val_nil(); fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); char* b = nr_alloc(sz + 1); fread(b, 1, sz, f); b[sz] = 0; fclose(f); return val_str(b); }
Value nr_rt_file_write(Value path, Value content) { if (path.type != VAL_STR || content.type != VAL_STR) return val_nil(); if (!is_safe_path(path.data.s)) return val_error("Path traversal detected"); FILE* f = fopen(path.data.s, "w"); if (!f) return val_nil(); fputs(content.data.s, f); fclose(f); return val_int(1); }
Value nr_rt_file_exists(Value path) { if (path.type != VAL_STR) return val_bool(false); if (!is_safe_path(path.data.s)) return val_bool(false); struct stat st; return val_bool(stat(path.data.s, &st) == 0); }
Value nr_rt_file_delete(Value path) { if (path.type != VAL_STR) return val_nil(); if (!is_safe_path(path.data.s)) return val_error("Path traversal detected"); remove(path.data.s); return val_int(1); }

Value nr_rt_now() { return val_int((int)time(NULL)); }
Value nr_rt_sleep(Value ms) { if (ms.type == VAL_INT) usleep(ms.data.i * 1000); return val_nil(); }
Value nr_rt_random() { return val_int(rand()); }

Value nr_rt_substring(Value s, Value start, Value len) { if (s.type != VAL_STR || start.type != VAL_INT || len.type != VAL_INT) return val_nil(); int slen = strlen(s.data.s); int istart = start.data.i; int ilen = len.data.i; if (istart < 0) istart = 0; if (istart >= slen) return val_str(""); if (ilen < 0) ilen = 0; if (istart + ilen > slen) ilen = slen - istart; char* sub = nr_alloc(ilen + 1); strncpy(sub, s.data.s + istart, ilen); sub[ilen] = 0; return val_str(sub); }

Value nr_rt_exec(Value cmd) { if (cmd.type != VAL_STR) return val_int(-1); for(char* p=cmd.data.s; *p; p++) { if(!isalnum(*p) && *p!='.' && *p!='/' && *p!='_' && *p!='-' && *p!=' ') return val_error("Illegal character in command"); } return val_int(system(cmd.data.s)); }
Value nr_rt_str_index_of(Value s, Value sub) { if (s.type != VAL_STR || sub.type != VAL_STR) return val_int(-1); char* p = strstr(s.data.s, sub.data.s); if (!p) return val_int(-1); int res = (int)(p - s.data.s); return val_int(res); }
Value nr_rt_str_match(Value s, Value regex_str) { if (s.type != VAL_STR || regex_str.type != VAL_STR) return val_bool(0); regex_t regex; int reti = regcomp(&regex, regex_str.data.s, REG_EXTENDED); if (reti) return val_bool(0); reti = regexec(&regex, s.data.s, 0, NULL, 0); regfree(&regex); return val_bool(!reti); }
Value nr_rt_args() { Value a = val_arr(); for(int i=0; i<nr_argc; i++) nr_rt_push(a, val_str(nr_argv[i])); return a; }
Value nr_rt_to_string(Value v) { char buf[64]; if (v.type == VAL_INT) sprintf(buf, "%d", v.data.i); else if (v.type == VAL_FLOAT) sprintf(buf, "%g", v.data.f); else if (v.type == VAL_BOOL) strcpy(buf, v.data.i ? "true" : "false"); else if (v.type == VAL_STR) return v; else if (v.type == VAL_NIL) strcpy(buf, "null"); else strcpy(buf, "[object]"); return val_str(nr_strdup(buf)); }
void nr_rt_exit(Value code) { exit(code.type == VAL_INT ? code.data.i : 0); }
Value nr_rt_str_replace(Value s, Value old, Value new_str) {
  if (s.type != VAL_STR || old.type != VAL_STR || new_str.type != VAL_STR) return s;
  char *res; int i, count = 0; int oldlen = strlen(old.data.s); int newlen = strlen(new_str.data.s);
  for (i = 0; s.data.s[i] != '\0'; i++) { if (strstr(&s.data.s[i], old.data.s) == &s.data.s[i]) { count++; i += oldlen - 1; } }
  res = (char *)nr_alloc(i + count * (newlen - oldlen) + 1);
  i = 0; while (*s.data.s) { if (strstr(s.data.s, old.data.s) == s.data.s) { strcpy(&res[i], new_str.data.s); i += newlen; s.data.s += oldlen; } else res[i++] = *s.data.s++; } res[i] = '\0'; return val_str(res);
}
char* val_to_json(Value v) { if (v.type == VAL_INT) { char* b = nr_alloc(32); sprintf(b, "%d", v.data.i); return b; } if (v.type == VAL_STR) { char* b = nr_alloc(strlen(v.data.s) + 3); sprintf(b, "\"%s\"", v.data.s); return b; } if (v.type == VAL_BOOL) return nr_strdup(v.data.i ? "true" : "false"); if (v.type == VAL_ARR) { char* res = nr_strdup("["); for (int i=0; i<v.data.arr->count; i++) { char* item = val_to_json(*v.data.arr->elements[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(item) + 3); sprintf(res, "%s%s%s", old, item, i < v.data.arr->count-1 ? "," : ""); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, "%s]", old); return res; } if (v.type == VAL_OBJ) { char* res = nr_strdup("{"); for (int i=0; i<v.data.obj->count; i++) { char* val = val_to_json(*v.data.obj->values[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(v.data.obj->keys[i]) + strlen(val) + 6); sprintf(res, "%s\"%s\":%s%s", old, v.data.obj->keys[i], val, i < v.data.obj->count-1 ? "," : ""); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, "%s}", old); return res; } return nr_strdup("null"); }
Value nr_rt_json_encode(Value v) { return val_str(val_to_json(v)); }

Value nr_rt_json_decode(Value s) {
  if (s.type != VAL_STR) return val_nil();
  char* p = s.data.s; while(*p == ' ' || *p == '[' || *p == ']') p++;
  if (*p == '{') {
    Value obj = val_obj(); p++;
    while(*p && *p != '}') {
      while(*p == ' ' || *p == '"' || *p == ',') p++;
      char key[64]; int i=0; while(*p && *p != '"') key[i++] = *p++; key[i] = 0; p++;
      while(*p == ' ' || *p == ':') p++;
      if (*p == '"') { p++; char val[256]; int j=0; while(*p && *p != '"') val[j++] = *p++; val[j] = 0; p++; set_field(obj, key, val_str(val)); }
      else { char val[32]; int j=0; while(*p && *p != ',' && *p != ' ' && *p != '}') val[j++] = *p++; val[j] = 0; set_field(obj, key, val_int(atoi(val))); }
    } return obj;
  } return val_nil();
}
Value nr_rt_http_serve(Value port, Value callback) {
  #ifdef _WIN32
  WSADATA wsaData; WSAStartup(MAKEWORD(2,2), &wsaData);
  #endif
  int server_fd, new_socket; struct sockaddr_in address; int addrlen = sizeof(address);
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
  address.sin_family = AF_INET; address.sin_addr.s_addr = INADDR_ANY; address.sin_port = htons(port.data.i);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind"); close(server_fd); return val_nil(); }
  if (listen(server_fd, 3) < 0) { perror("listen"); close(server_fd); return val_nil(); }
  printf("HTTP Server listening on port %d...\n", port.data.i);
  while(1) {
    void* cp = nr_checkpoint();
    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    char buffer[30000] = {0}; int n = recv(new_socket, buffer, 30000, 0);
    if (n <= 0) { close(new_socket); nr_rollback(cp); continue; }
    char method[16]={0}, path[1024]={0};
    if(sscanf(buffer, "%15s %1023s", method, path) < 2) { close(new_socket); nr_rollback(cp); continue; }
    Value req = val_obj(); set_field(req, "method", val_str(method)); set_field(req, "path", val_str(path));
    char* body_ptr = strstr(buffer, "\r\n\r\n"); if (body_ptr) { body_ptr += 4; set_field(req, "body", val_str(body_ptr)); } else { set_field(req, "body", val_nil()); }
    Value res = ((Value (*)(Value, Value, Value, Value, Value, Value))callback.data.func_ptr)(val_nil(), req, val_nil(), val_nil(), val_nil(), val_nil());
    char* res_body = "Not Found"; int status = 404;
    if (res.type == VAL_OBJ) {
      Value b = get_field(res, "body"); if (b.type == VAL_STR) res_body = b.data.s;
      Value s = get_field(res, "status"); if (s.type == VAL_INT) status = s.data.i;
    }
    char response[32000];
    snprintf(response, sizeof(response), "HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %ld\r\n\r\n%s", status, strlen(res_body), res_body);
    send(new_socket, response, strlen(response), 0); close(new_socket);
    nr_rollback(cp);
  } return val_nil();
}
void nr_api_init();
void nr_http_init();
void nr_string_init();
Value nr_string_indexOf(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_replace(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_match(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_contains(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_split(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_trim(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_upper(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_string_lower(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_http_app(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_1(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_2(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_3(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_4(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_5(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_http_serve(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_http_match_path(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_http_create_ctx(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_6(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_lambda_7(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
void nr_task_init();
void nr_sqlite3_init();

    static void bind_val(sqlite3_stmt* stmt, int idx, Value v) {
      if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, idx, v.data.i);
      else if (v.type == VAL_STR) sqlite3_bind_text(stmt, idx, v.data.s, -1, SQLITE_TRANSIENT);
      else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, idx);
    }
    Value __native_sqlite3_open(Value path) { return path; }
    Value __native_sqlite3_exec(Value db, Value sql, Value params) {
      if (sql.type != VAL_STR || db.type != VAL_STR) return val_int(0);
      sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return val_int(0);
      sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
        if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }
        sqlite3_step(stmt); sqlite3_finalize(stmt);
      }
      sqlite3_close(h); return val_int(1);
    }
    Value __native_sqlite3_query(Value db, Value sql, Value params) {
      Value a = val_arr(); if (sql.type != VAL_STR || db.type != VAL_STR) return a;
      sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return a;
      sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
        if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }
        int cols = sqlite3_column_count(stmt);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
          Value o = val_obj();
          for (int i=0; i<cols; i++) {
            const char* name = sqlite3_column_name(stmt, i); int type = sqlite3_column_type(stmt, i);
            Value v = val_nil();
            if (type == SQLITE_INTEGER) v = val_int(sqlite3_column_int(stmt, i));
            else if (type == SQLITE_NULL) v = val_nil();
            else v = val_str(nr_strdup((char*)sqlite3_column_text(stmt, i)));
            set_field(o, name, v);
          }
          nr_rt_push(a, o);
        }
        sqlite3_finalize(stmt);
      }
      sqlite3_close(h); return a;
    }
  
Value nr_sqlite3_open(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_sqlite3_exec(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_sqlite3_query(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
void nr_sys_init();
Value nr_sys_args(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_sys_now(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_sys_exit(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_prepare(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_all(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_find(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_create(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_update(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_task_delete(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_main(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_get_tasks(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_get_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_create_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_update_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_api_delete_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
void nr_cli_init();
Value nr_cli_run(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5);
Value nr_v_api;
Value nr_v_http;
Value nr_v_string;
Value nr_v_string_indexOf;
Value nr_v_s;
Value nr_v_sub;
Value nr_v_string_replace;
Value nr_v_old;
Value nr_v_new_str;
Value nr_v_string_match;
Value nr_v_regex;
Value nr_v_string_contains;
Value nr_v_string_split;
Value nr_v_sep;
Value nr_v_res;
Value nr_v_curr;
Value nr_v_idx;
Value nr_v_part;
Value nr_v_string_trim;
Value nr_v_string_upper;
Value nr_v_string_lower;
Value nr_v__routes;
Value nr_v__serve;
Value nr_v_http_app;
Value nr_v_lambda_1;
Value nr_v_path;
Value nr_v_handler;
Value nr_v_lambda_2;
Value nr_v_lambda_3;
Value nr_v_lambda_4;
Value nr_v_lambda_5;
Value nr_v_port;
Value nr_v_http_serve;
Value nr_v_req;
Value nr_v_r;
Value nr_v_ctx;
Value nr_v_http_match_path;
Value nr_v_pattern;
Value nr_v_actual_path;
Value nr_v_colon_idx;
Value nr_v_pattern_base;
Value nr_v_path_base;
Value nr_v_http_create_ctx;
Value nr_v_params;
Value nr_v_lambda_6;
Value nr_v_data;
Value nr_v_lambda_7;
Value nr_v_task;
Value nr_v_sqlite3;
Value nr_v_sqlite3_open;
Value nr_v_sqlite3_exec;
Value nr_v_db;
Value nr_v_sql;
Value nr_v_sqlite3_query;
Value nr_v_sys;
Value nr_v_sys_args;
Value nr_v_sys_now;
Value nr_v_sys_exit;
Value nr_v_code;
Value nr_v__db;
Value nr_v_task_prepare;
Value nr_v_task_all;
Value nr_v_task_find;
Value nr_v_id;
Value nr_v_task_create;
Value nr_v_title;
Value nr_v_task_update;
Value nr_v_done;
Value nr_v_task_delete;
Value nr_v_api_main;
Value nr_v_app;
Value nr_v_api_get_tasks;
Value nr_v_api_get_task;
Value nr_v_t;
Value nr_v_api_create_task;
Value nr_v_api_update_task;
Value nr_v_api_delete_task;
Value nr_v_cli;
Value nr_v_cli_run;
Value nr_v_args;
Value nr_v_cmd;
Value nr_v_arg_offset;
Value nr_v_items;
Value nr_v_status;

Value nr_string_indexOf(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  Value nr_v_sub = nr_v_a2;
  return nr_rt_str_index_of(nr_v_s, nr_v_sub);

  return val_nil(); }

Value nr_string_replace(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  Value nr_v_old = nr_v_a2;
  Value nr_v_new_str = nr_v_a3;
  return nr_rt_str_replace(nr_v_s, nr_v_old, nr_v_new_str);

  return val_nil(); }

Value nr_string_match(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  Value nr_v_regex = nr_v_a2;
  return nr_rt_str_match(nr_v_s, nr_v_regex);

  return val_nil(); }

Value nr_string_contains(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  Value nr_v_sub = nr_v_a2;
  return val_bool(!is_truthy(nr_rt_eq(nr_string_indexOf(val_nil(), nr_v_s, nr_v_sub, val_nil(), val_nil(), val_nil()), val_int(val_int(0).data.i - val_int(1).data.i))));

  return val_nil(); }

Value nr_string_split(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  Value nr_v_sep = nr_v_a2;
  nr_v_res = ({ Value _a = val_arr(); _a; });
  nr_v_curr = nr_v_s;
  nr_v_idx = nr_string_indexOf(val_nil(), nr_v_curr, nr_v_sep, val_nil(), val_nil(), val_nil());
  while (is_truthy(val_bool(!is_truthy(nr_rt_eq(nr_v_idx, val_int(val_int(0).data.i - val_int(1).data.i)))))) {
  nr_v_part = ({ Value _t = nr_v_curr; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(0), nr_v_idx, val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(0), nr_v_idx); } _r; });
({ Value _t = nr_v_res; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_part, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, nr_v_part); } _r; });
  nr_v_curr = ({ Value _t = nr_v_curr; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_rt_add(nr_v_idx, nr_rt_len(nr_v_sep)), nr_rt_len(nr_v_curr), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, nr_rt_add(nr_v_idx, nr_rt_len(nr_v_sep)), nr_rt_len(nr_v_curr)); } _r; });
  nr_v_idx = nr_string_indexOf(val_nil(), nr_v_curr, nr_v_sep, val_nil(), val_nil(), val_nil());
  }
;
({ Value _t = nr_v_res; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_curr, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, nr_v_curr); } _r; });
  return nr_v_res;

  return val_nil(); }

Value nr_string_trim(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  nr_v_res = nr_v_s;
  while (is_truthy(({ Value _l = val_int(nr_rt_len(nr_v_res).data.i > val_int(0).data.i); is_truthy(_l) ? nr_rt_eq(({ Value _t = nr_v_res; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(0), val_int(1), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(0), val_int(1)); } _r; }), val_str(" ")) : _l; }))) {
  nr_v_res = ({ Value _t = nr_v_res; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(1), nr_rt_len(nr_v_res), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(1), nr_rt_len(nr_v_res)); } _r; });
  }
;
  while (is_truthy(({ Value _l = val_int(nr_rt_len(nr_v_res).data.i > val_int(0).data.i); is_truthy(_l) ? nr_rt_eq(({ Value _t = nr_v_res; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(nr_rt_len(nr_v_res).data.i - val_int(1).data.i), val_int(1), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(nr_rt_len(nr_v_res).data.i - val_int(1).data.i), val_int(1)); } _r; }), val_str(" ")) : _l; }))) {
  nr_v_res = ({ Value _t = nr_v_res; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(0), val_int(nr_rt_len(nr_v_res).data.i - val_int(1).data.i), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(0), val_int(nr_rt_len(nr_v_res).data.i - val_int(1).data.i)); } _r; });
  }
;
  return nr_v_res;

  return val_nil(); }

Value nr_string_upper(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  return nr_v_s;

  return val_nil(); }

Value nr_string_lower(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_s = nr_v_a1;
  return nr_v_s;

  return val_nil(); }

void nr_string_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_string.type == VAL_NIL) nr_v_string = val_obj();
  set_field(nr_v_string, "indexOf", val_func(nr_string_indexOf));
  set_field(nr_v_string, "replace", val_func(nr_string_replace));
  set_field(nr_v_string, "match", val_func(nr_string_match));
  set_field(nr_v_string, "contains", val_func(nr_string_contains));
  set_field(nr_v_string, "split", val_func(nr_string_split));
  set_field(nr_v_string, "trim", val_func(nr_string_trim));
  set_field(nr_v_string, "upper", val_func(nr_string_upper));
  set_field(nr_v_string, "lower", val_func(nr_string_lower));
val_func(nr_string_indexOf);
val_func(nr_string_replace);
val_func(nr_string_match);
val_func(nr_string_contains);
val_func(nr_string_split);
val_func(nr_string_trim);
val_func(nr_string_upper);
val_func(nr_string_lower);
}

Value nr_http_app(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  nr_v__serve = val_func(nr_http_serve);
  return ({ Value _o = val_obj(); set_field(_o, "routes", nr_v__routes); set_field(_o, "get", val_func(nr_lambda_1)); set_field(_o, "post", val_func(nr_lambda_2)); set_field(_o, "put", val_func(nr_lambda_3)); set_field(_o, "delete", val_func(nr_lambda_4)); set_field(_o, "listen", val_func(nr_lambda_5)); _o; });

  return val_nil(); }

Value nr_lambda_1(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_path = nr_v_a1;
  Value nr_v_handler = nr_v_a2;
  return ({ Value _t = nr_v__routes; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("GET")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("GET")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; })); } _r; });
  return val_nil(); }

Value nr_lambda_2(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_path = nr_v_a1;
  Value nr_v_handler = nr_v_a2;
  return ({ Value _t = nr_v__routes; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("POST")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("POST")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; })); } _r; });
  return val_nil(); }

Value nr_lambda_3(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_path = nr_v_a1;
  Value nr_v_handler = nr_v_a2;
  return ({ Value _t = nr_v__routes; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("PUT")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("PUT")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; })); } _r; });
  return val_nil(); }

Value nr_lambda_4(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_path = nr_v_a1;
  Value nr_v_handler = nr_v_a2;
  return ({ Value _t = nr_v__routes; Value _f = get_field(_t, "push"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("DELETE")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_push(_t, ({ Value _o = val_obj(); set_field(_o, "method", val_str("DELETE")); set_field(_o, "path", nr_v_path); set_field(_o, "handler", nr_v_handler); _o; })); } _r; });
  return val_nil(); }

Value nr_lambda_5(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_port = nr_v_a1;
  return nr_rt_http_serve(nr_v_port, nr_v__serve);
  return val_nil(); }

Value nr_http_serve(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_req = nr_v_a1;
  if (nr_v__routes.type == VAL_ARR) {
  for (int _i8 = 0; _i8 < nr_v__routes.data.arr->count; _i8++) {
    nr_v_r = *nr_v__routes.data.arr->elements[_i8];
  if (is_truthy(({ Value _l = nr_rt_eq(get_field(nr_v_r, "method"), get_field(nr_v_req, "method")); is_truthy(_l) ? nr_http_match_path(val_nil(), get_field(nr_v_r, "path"), get_field(nr_v_req, "path"), val_nil(), val_nil(), val_nil()) : _l; }))) {
  nr_v_ctx = nr_http_create_ctx(val_nil(), nr_v_req, get_field(nr_v_r, "path"), val_nil(), val_nil(), val_nil());
  return ({ Value _t = nr_v_r; Value _f = get_field(_t, "handler"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_ctx, val_nil(), val_nil(), val_nil(), val_nil()); else { if (nr_v_handler.type == VAL_FUNC && nr_v_handler.data.func_ptr) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))nr_v_handler.data.func_ptr)(val_nil(), _t, val_nil(), val_nil(), val_nil(), val_nil()); else _r = val_nil(); } _r; });
  };
  }
  }
;
  return ({ Value _o = val_obj(); set_field(_o, "status", val_int(404)); set_field(_o, "body", val_str("Not Found")); _o; });

  return val_nil(); }

Value nr_http_match_path(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_pattern = nr_v_a1;
  Value nr_v_actual_path = nr_v_a2;
  if (is_truthy(nr_rt_eq(nr_v_pattern, nr_v_actual_path))) {
  return val_bool(1);
  };
  nr_v_colon_idx = ({ Value _t = nr_v_string; Value _f = get_field(_t, "indexOf"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_pattern, val_str(":"), val_nil(), val_nil(), val_nil()); else { _r = nr_string_indexOf(_t, nr_v_pattern, val_str(":"), val_nil(), val_nil(), val_nil()); } _r; });
  if (is_truthy(val_int(nr_v_colon_idx.data.i > val_int(0).data.i))) {
  nr_v_pattern_base = ({ Value _t = nr_v_pattern; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(0), nr_v_colon_idx, val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(0), nr_v_colon_idx); } _r; });
  if (is_truthy(val_int(nr_rt_len(nr_v_actual_path).data.i >= nr_v_colon_idx.data.i))) {
  nr_v_path_base = ({ Value _t = nr_v_actual_path; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(0), nr_v_colon_idx, val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, val_int(0), nr_v_colon_idx); } _r; });
  if (is_truthy(nr_rt_eq(nr_v_pattern_base, nr_v_path_base))) {
  return val_bool(1);
  };
  };
  };
  return val_bool(0);

  return val_nil(); }

Value nr_http_create_ctx(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_req = nr_v_a1;
  Value nr_v_pattern = nr_v_a2;
  nr_v_params = ({ Value _o = val_obj(); _o; });
  nr_v_actual_path = get_field(nr_v_req, "path");
  nr_v_colon_idx = ({ Value _t = nr_v_string; Value _f = get_field(_t, "indexOf"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_pattern, val_str(":"), val_nil(), val_nil(), val_nil()); else { _r = nr_string_indexOf(_t, nr_v_pattern, val_str(":"), val_nil(), val_nil(), val_nil()); } _r; });
  if (is_truthy(val_int(nr_v_colon_idx.data.i > val_int(0).data.i))) {
  set_field(nr_v_params, "id", ({ Value _t = nr_v_actual_path; Value _f = get_field(_t, "substring"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_colon_idx, val_int(10), val_nil(), val_nil(), val_nil()); else { _r = nr_rt_substring(_t, nr_v_colon_idx, val_int(10)); } _r; }));
;
  };
  return ({ Value _o = val_obj(); set_field(_o, "method", get_field(nr_v_req, "method")); set_field(_o, "path", get_field(nr_v_req, "path")); set_field(_o, "params", nr_v_params); set_field(_o, "body", get_field(nr_v_req, "body")); set_field(_o, "json", val_func(nr_lambda_6)); set_field(_o, "text", val_func(nr_lambda_7)); _o; });

  return val_nil(); }

Value nr_lambda_6(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_data = nr_v_a1;
  return ({ Value _o = val_obj(); set_field(_o, "status", val_int(200)); set_field(_o, "headers", ({ Value _o = val_obj(); set_field(_o, "Content-Type", val_str("application/json")); _o; })); set_field(_o, "body", nr_rt_json_encode(nr_v_data)); _o; });
  return val_nil(); }

Value nr_lambda_7(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_data = nr_v_a1;
  return ({ Value _o = val_obj(); set_field(_o, "status", val_int(200)); set_field(_o, "headers", ({ Value _o = val_obj(); set_field(_o, "Content-Type", val_str("text/plain")); _o; })); set_field(_o, "body", nr_v_data); _o; });
  return val_nil(); }

void nr_http_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_http.type == VAL_NIL) nr_v_http = val_obj();
  set_field(nr_v_http, "app", val_func(nr_http_app));
  set_field(nr_v_http, "serve", val_func(nr_http_serve));
  set_field(nr_v_http, "path", val_func(nr_http_match_path));
  set_field(nr_v_http, "ctx", val_func(nr_http_create_ctx));
  nr_string_init();
  if (nr_v_string.type == VAL_NIL) nr_v_string = val_obj();
  set_field(nr_v_string, "indexOf", val_func(nr_string_indexOf));
  set_field(nr_v_string, "replace", val_func(nr_string_replace));
  set_field(nr_v_string, "match", val_func(nr_string_match));
  set_field(nr_v_string, "contains", val_func(nr_string_contains));
  set_field(nr_v_string, "split", val_func(nr_string_split));
  set_field(nr_v_string, "trim", val_func(nr_string_trim));
  set_field(nr_v_string, "upper", val_func(nr_string_upper));
  set_field(nr_v_string, "lower", val_func(nr_string_lower));
;
  nr_v__routes = ({ Value _a = val_arr(); _a; });
  nr_v__serve = val_nil();
val_func(nr_http_app);
val_func(nr_http_serve);
val_func(nr_http_match_path);
val_func(nr_http_create_ctx);
}

Value nr_sqlite3_open(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_path = nr_v_a1;
  return __native_sqlite3_open(nr_v_path);

  return val_nil(); }

Value nr_sqlite3_exec(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_db = nr_v_a1;
  Value nr_v_sql = nr_v_a2;
  Value nr_v_params = nr_v_a3;
  return __native_sqlite3_exec(nr_v_db, nr_v_sql, ({ Value _l = nr_v_params; is_truthy(_l) ? _l : ({ Value _a = val_arr(); _a; }); }));

  return val_nil(); }

Value nr_sqlite3_query(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_db = nr_v_a1;
  Value nr_v_sql = nr_v_a2;
  Value nr_v_params = nr_v_a3;
  return __native_sqlite3_query(nr_v_db, nr_v_sql, ({ Value _l = nr_v_params; is_truthy(_l) ? _l : ({ Value _a = val_arr(); _a; }); }));

  return val_nil(); }

void nr_sqlite3_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_sqlite3.type == VAL_NIL) nr_v_sqlite3 = val_obj();
  set_field(nr_v_sqlite3, "open", val_func(nr_sqlite3_open));
  set_field(nr_v_sqlite3, "exec", val_func(nr_sqlite3_exec));
  set_field(nr_v_sqlite3, "query", val_func(nr_sqlite3_query));
;
val_func(nr_sqlite3_open);
val_func(nr_sqlite3_exec);
val_func(nr_sqlite3_query);
}

Value nr_sys_args(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  return nr_rt_args();

  return val_nil(); }

Value nr_sys_now(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  return nr_rt_now();

  return val_nil(); }

Value nr_sys_exit(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_code = nr_v_a1;
nr_rt_exit(nr_v_code);

  return val_nil(); }

void nr_sys_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_sys.type == VAL_NIL) nr_v_sys = val_obj();
  set_field(nr_v_sys, "args", val_func(nr_sys_args));
  set_field(nr_v_sys, "now", val_func(nr_sys_now));
  set_field(nr_v_sys, "exit", val_func(nr_sys_exit));
val_func(nr_sys_args);
val_func(nr_sys_now);
val_func(nr_sys_exit);
}

Value nr_task_prepare(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  if (is_truthy(nr_rt_eq(nr_v__db, val_nil()))) {
  nr_v__db = ({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "open"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("tasks.db"), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_sqlite3_open(_t, val_str("tasks.db"), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "exec"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, done BOOLEAN, created_at INTEGER)"), val_nil(), val_nil(), val_nil()); else { _r = nr_sqlite3_exec(_t, nr_v__db, val_str("CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, done BOOLEAN, created_at INTEGER)"), val_nil(), val_nil(), val_nil()); } _r; });
  };

  return val_nil(); }

Value nr_task_all(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
nr_task_prepare(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil());
  return ({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "query"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("SELECT * FROM tasks"), val_nil(), val_nil(), val_nil()); else { _r = nr_sqlite3_query(_t, nr_v__db, val_str("SELECT * FROM tasks"), val_nil(), val_nil(), val_nil()); } _r; });

  return val_nil(); }

Value nr_task_find(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_id = nr_v_a1;
nr_task_prepare(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil());
  return ({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "query"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("SELECT * FROM tasks WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); else { _r = nr_sqlite3_query(_t, nr_v__db, val_str("SELECT * FROM tasks WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); } _r; });

  return val_nil(); }

Value nr_task_create(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_title = nr_v_a1;
nr_task_prepare(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil());
({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "exec"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("INSERT INTO tasks (title, done, created_at) VALUES (?, 0, ?)"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_v_title); nr_rt_push(_a, nr_rt_now()); _a; }), val_nil(), val_nil()); else { _r = nr_sqlite3_exec(_t, nr_v__db, val_str("INSERT INTO tasks (title, done, created_at) VALUES (?, 0, ?)"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_v_title); nr_rt_push(_a, nr_rt_now()); _a; }), val_nil(), val_nil()); } _r; });

  return val_nil(); }

Value nr_task_update(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_id = nr_v_a1;
  Value nr_v_title = nr_v_a2;
  Value nr_v_done = nr_v_a3;
nr_task_prepare(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil());
({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "exec"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("UPDATE tasks SET title = ?, done = ? WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_v_title); nr_rt_push(_a, nr_v_done); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); else { _r = nr_sqlite3_exec(_t, nr_v__db, val_str("UPDATE tasks SET title = ?, done = ? WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_v_title); nr_rt_push(_a, nr_v_done); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); } _r; });

  return val_nil(); }

Value nr_task_delete(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_id = nr_v_a1;
nr_task_prepare(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil());
({ Value _t = nr_v_sqlite3; Value _f = get_field(_t, "exec"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v__db, val_str("DELETE FROM tasks WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); else { _r = nr_sqlite3_exec(_t, nr_v__db, val_str("DELETE FROM tasks WHERE id = ?"), ({ Value _a = val_arr(); nr_rt_push(_a, nr_rt_to_int(nr_v_id)); _a; }), val_nil(), val_nil()); } _r; });

  return val_nil(); }

void nr_task_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_task.type == VAL_NIL) nr_v_task = val_obj();
  set_field(nr_v_task, "prepare", val_func(nr_task_prepare));
  set_field(nr_v_task, "all", val_func(nr_task_all));
  set_field(nr_v_task, "find", val_func(nr_task_find));
  set_field(nr_v_task, "create", val_func(nr_task_create));
  set_field(nr_v_task, "update", val_func(nr_task_update));
  set_field(nr_v_task, "delete", val_func(nr_task_delete));
  nr_sqlite3_init();
  if (nr_v_sqlite3.type == VAL_NIL) nr_v_sqlite3 = val_obj();
  set_field(nr_v_sqlite3, "open", val_func(nr_sqlite3_open));
  set_field(nr_v_sqlite3, "exec", val_func(nr_sqlite3_exec));
  set_field(nr_v_sqlite3, "query", val_func(nr_sqlite3_query));
;
  nr_sys_init();
  if (nr_v_sys.type == VAL_NIL) nr_v_sys = val_obj();
  set_field(nr_v_sys, "args", val_func(nr_sys_args));
  set_field(nr_v_sys, "now", val_func(nr_sys_now));
  set_field(nr_v_sys, "exit", val_func(nr_sys_exit));
;
  nr_v__db = val_nil();
val_func(nr_task_prepare);
val_func(nr_task_all);
val_func(nr_task_find);
val_func(nr_task_create);
val_func(nr_task_update);
val_func(nr_task_delete);
}

Value nr_api_main(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  nr_v_app = ({ Value _t = nr_v_http; Value _f = get_field(_t, "app"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_http_app(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
({ Value _t = nr_v_app; Value _f = get_field(_t, "get"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("/tasks"), val_func(nr_api_get_tasks), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
({ Value _t = nr_v_app; Value _f = get_field(_t, "get"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("/tasks/:id"), val_func(nr_api_get_task), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
({ Value _t = nr_v_app; Value _f = get_field(_t, "post"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("/tasks"), val_func(nr_api_create_task), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
({ Value _t = nr_v_app; Value _f = get_field(_t, "put"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("/tasks/:id"), val_func(nr_api_update_task), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
({ Value _t = nr_v_app; Value _f = get_field(_t, "delete"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_str("/tasks/:id"), val_func(nr_api_delete_task), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
nr_rt_print(val_str("Server running on http://localhost:3000"));
({ Value _t = nr_v_app; Value _f = get_field(_t, "listen"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_int(3000), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });

  return val_nil(); }

Value nr_api_get_tasks(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_ctx = nr_v_a1;
  return ({ Value _t = nr_v_ctx; Value _f = get_field(_t, "json"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _t = nr_v_task; Value _f = get_field(_t, "all"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_all(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });

  return val_nil(); }

Value nr_api_get_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_ctx = nr_v_a1;
  nr_v_id = get_field(get_field(nr_v_ctx, "params"), "id");
  nr_v_t = ({ Value _t = nr_v_task; Value _f = get_field(_t, "find"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_find(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  if (is_truthy(val_bool(!is_truthy(nr_rt_eq(nr_v_t, val_nil()))))) {
  return ({ Value _t = nr_v_ctx; Value _f = get_field(_t, "json"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_t, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });
  };
  return ({ Value _o = val_obj(); set_field(_o, "status", val_int(404)); set_field(_o, "body", val_str("Task not found")); _o; });

  return val_nil(); }

Value nr_api_create_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_ctx = nr_v_a1;
  nr_v_data = nr_rt_json_decode(get_field(nr_v_ctx, "body"));
({ Value _t = nr_v_task; Value _f = get_field(_t, "create"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, get_field(nr_v_data, "title"), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_create(_t, get_field(nr_v_data, "title"), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  return ({ Value _t = nr_v_ctx; Value _f = get_field(_t, "json"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "ok", val_bool(1)); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });

  return val_nil(); }

Value nr_api_update_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_ctx = nr_v_a1;
  nr_v_id = get_field(get_field(nr_v_ctx, "params"), "id");
  nr_v_data = nr_rt_json_decode(get_field(nr_v_ctx, "body"));
({ Value _t = nr_v_task; Value _f = get_field(_t, "update"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, get_field(nr_v_data, "title"), get_field(nr_v_data, "done"), val_nil(), val_nil()); else { _r = nr_task_update(_t, nr_v_id, get_field(nr_v_data, "title"), get_field(nr_v_data, "done"), val_nil(), val_nil()); } _r; });
  return ({ Value _t = nr_v_ctx; Value _f = get_field(_t, "json"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "ok", val_bool(1)); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });

  return val_nil(); }

Value nr_api_delete_task(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_ctx = nr_v_a1;
  nr_v_id = get_field(get_field(nr_v_ctx, "params"), "id");
({ Value _t = nr_v_task; Value _f = get_field(_t, "delete"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_delete(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  return ({ Value _t = nr_v_ctx; Value _f = get_field(_t, "json"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, ({ Value _o = val_obj(); set_field(_o, "ok", val_bool(1)); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = val_nil(); } _r; });

  return val_nil(); }

void nr_api_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_api.type == VAL_NIL) nr_v_api = val_obj();
  set_field(nr_v_api, "main", val_func(nr_api_main));
  set_field(nr_v_api, "tasks", val_func(nr_api_get_tasks));
  set_field(nr_v_api, "task", val_func(nr_api_get_task));
  set_field(nr_v_api, "task", val_func(nr_api_create_task));
  set_field(nr_v_api, "task", val_func(nr_api_update_task));
  set_field(nr_v_api, "task", val_func(nr_api_delete_task));
  nr_http_init();
  if (nr_v_http.type == VAL_NIL) nr_v_http = val_obj();
  set_field(nr_v_http, "app", val_func(nr_http_app));
  set_field(nr_v_http, "serve", val_func(nr_http_serve));
  set_field(nr_v_http, "path", val_func(nr_http_match_path));
  set_field(nr_v_http, "ctx", val_func(nr_http_create_ctx));
;
  nr_task_init();
  if (nr_v_task.type == VAL_NIL) nr_v_task = val_obj();
  set_field(nr_v_task, "prepare", val_func(nr_task_prepare));
  set_field(nr_v_task, "all", val_func(nr_task_all));
  set_field(nr_v_task, "find", val_func(nr_task_find));
  set_field(nr_v_task, "create", val_func(nr_task_create));
  set_field(nr_v_task, "update", val_func(nr_task_update));
  set_field(nr_v_task, "delete", val_func(nr_task_delete));
;
val_func(nr_api_main);
val_func(nr_api_get_tasks);
val_func(nr_api_get_task);
val_func(nr_api_create_task);
val_func(nr_api_update_task);
val_func(nr_api_delete_task);
}

Value nr_cli_run(Value self, Value nr_v_a1, Value nr_v_a2, Value nr_v_a3, Value nr_v_a4, Value nr_v_a5) {
  Value nr_v_args = nr_v_a1;
  nr_v_cmd = val_str("");
  nr_v_arg_offset = val_int(0);
  if (is_truthy(({ Value _l = nr_rt_eq(nr_rt_at(nr_v_args, val_int(0)), val_str("./nira")); is_truthy(_l) ? _l : nr_rt_eq(nr_rt_at(nr_v_args, val_int(0)), val_str("nira")); }))) {
  nr_v_cmd = nr_rt_at(nr_v_args, val_int(3));
  nr_v_arg_offset = val_int(3);
  } else {
  nr_v_cmd = nr_rt_at(nr_v_args, val_int(1));
  nr_v_arg_offset = val_int(1);
  }
;
  if (is_truthy(nr_rt_eq(nr_v_cmd, val_str("add")))) {
  nr_v_title = nr_rt_at(nr_v_args, nr_rt_add(nr_v_arg_offset, val_int(1)));
({ Value _t = nr_v_task; Value _f = get_field(_t, "create"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_title, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_create(_t, nr_v_title, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
nr_rt_print(nr_rt_add(val_str("Task added: "), nr_v_title));
  };
  if (is_truthy(nr_rt_eq(nr_v_cmd, val_str("list")))) {
  nr_v_items = ({ Value _t = nr_v_task; Value _f = get_field(_t, "all"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_all(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
nr_rt_print(val_str("--- Tasks ---"));
  if (nr_v_items.type == VAL_ARR) {
  for (int _i9 = 0; _i9 < nr_v_items.data.arr->count; _i9++) {
    nr_v_t = *nr_v_items.data.arr->elements[_i9];
  nr_v_status = val_str("[ ]");
  if (is_truthy(get_field(nr_v_t, "done"))) {
  nr_v_status = val_str("[x]");
  };
nr_rt_print(nr_rt_add(nr_rt_add(nr_rt_add(nr_rt_add(nr_v_status, val_str(" ")), nr_rt_to_string(get_field(nr_v_t, "id"))), val_str(": ")), get_field(nr_v_t, "title")));
  }
  }
;
  };
  if (is_truthy(nr_rt_eq(nr_v_cmd, val_str("done")))) {
  nr_v_id = nr_rt_at(nr_v_args, nr_rt_add(nr_v_arg_offset, val_int(1)));
  nr_v_t = ({ Value _t = nr_v_task; Value _f = get_field(_t, "find"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_find(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  if (is_truthy(val_int(nr_rt_len(nr_v_t).data.i > val_int(0).data.i))) {
({ Value _t = nr_v_task; Value _f = get_field(_t, "update"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, nr_rt_at(nr_rt_at(nr_v_t, val_int(0)), val_str("title")), val_int(1), val_nil(), val_nil()); else { _r = nr_task_update(_t, nr_v_id, nr_rt_at(nr_rt_at(nr_v_t, val_int(0)), val_str("title")), val_int(1), val_nil(), val_nil()); } _r; });
nr_rt_print(nr_rt_add(nr_rt_add(val_str("Task "), nr_v_id), val_str(" marked as done")));
  };
  };
  if (is_truthy(nr_rt_eq(nr_v_cmd, val_str("delete")))) {
  nr_v_id = nr_rt_at(nr_v_args, nr_rt_add(nr_v_arg_offset, val_int(1)));
({ Value _t = nr_v_task; Value _f = get_field(_t, "delete"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_task_delete(_t, nr_v_id, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
nr_rt_print(nr_rt_add(nr_rt_add(val_str("Task "), nr_v_id), val_str(" deleted")));
  };

  return val_nil(); }

void nr_cli_init() { static bool init = false; if (init) return; init = true;
  if (nr_v_cli.type == VAL_NIL) nr_v_cli = val_obj();
  set_field(nr_v_cli, "run", val_func(nr_cli_run));
  nr_task_init();
  if (nr_v_task.type == VAL_NIL) nr_v_task = val_obj();
  set_field(nr_v_task, "prepare", val_func(nr_task_prepare));
  set_field(nr_v_task, "all", val_func(nr_task_all));
  set_field(nr_v_task, "find", val_func(nr_task_find));
  set_field(nr_v_task, "create", val_func(nr_task_create));
  set_field(nr_v_task, "update", val_func(nr_task_update));
  set_field(nr_v_task, "delete", val_func(nr_task_delete));
;
  nr_sys_init();
  if (nr_v_sys.type == VAL_NIL) nr_v_sys = val_obj();
  set_field(nr_v_sys, "args", val_func(nr_sys_args));
  set_field(nr_v_sys, "now", val_func(nr_sys_now));
  set_field(nr_v_sys, "exit", val_func(nr_sys_exit));
;
val_func(nr_cli_run);
nr_cli_run(val_nil(), ({ Value _t = nr_v_sys; Value _f = get_field(_t, "args"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_sys_args(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; }), val_nil(), val_nil(), val_nil(), val_nil());
}


int main(int argc, char** argv) {
  nr_arena = malloc(sizeof(Arena)); nr_arena->blocks = NULL;
  srand(time(NULL));
  nr_argc = argc; nr_argv = argv;
  Value self = val_nil(); (void)self;
  nr_v_api = val_nil();
  nr_v_http = val_nil();
  nr_v_string = val_nil();
  nr_v_string_indexOf = val_nil();
  nr_v_s = val_nil();
  nr_v_sub = val_nil();
  nr_v_string_replace = val_nil();
  nr_v_old = val_nil();
  nr_v_new_str = val_nil();
  nr_v_string_match = val_nil();
  nr_v_regex = val_nil();
  nr_v_string_contains = val_nil();
  nr_v_string_split = val_nil();
  nr_v_sep = val_nil();
  nr_v_res = val_nil();
  nr_v_curr = val_nil();
  nr_v_idx = val_nil();
  nr_v_part = val_nil();
  nr_v_string_trim = val_nil();
  nr_v_string_upper = val_nil();
  nr_v_string_lower = val_nil();
  nr_v__routes = val_nil();
  nr_v__serve = val_nil();
  nr_v_http_app = val_nil();
  nr_v_lambda_1 = val_nil();
  nr_v_path = val_nil();
  nr_v_handler = val_nil();
  nr_v_lambda_2 = val_nil();
  nr_v_lambda_3 = val_nil();
  nr_v_lambda_4 = val_nil();
  nr_v_lambda_5 = val_nil();
  nr_v_port = val_nil();
  nr_v_http_serve = val_nil();
  nr_v_req = val_nil();
  nr_v_r = val_nil();
  nr_v_ctx = val_nil();
  nr_v_http_match_path = val_nil();
  nr_v_pattern = val_nil();
  nr_v_actual_path = val_nil();
  nr_v_colon_idx = val_nil();
  nr_v_pattern_base = val_nil();
  nr_v_path_base = val_nil();
  nr_v_http_create_ctx = val_nil();
  nr_v_params = val_nil();
  nr_v_lambda_6 = val_nil();
  nr_v_data = val_nil();
  nr_v_lambda_7 = val_nil();
  nr_v_task = val_nil();
  nr_v_sqlite3 = val_nil();
  nr_v_sqlite3_open = val_nil();
  nr_v_sqlite3_exec = val_nil();
  nr_v_db = val_nil();
  nr_v_sql = val_nil();
  nr_v_sqlite3_query = val_nil();
  nr_v_sys = val_nil();
  nr_v_sys_args = val_nil();
  nr_v_sys_now = val_nil();
  nr_v_sys_exit = val_nil();
  nr_v_code = val_nil();
  nr_v__db = val_nil();
  nr_v_task_prepare = val_nil();
  nr_v_task_all = val_nil();
  nr_v_task_find = val_nil();
  nr_v_id = val_nil();
  nr_v_task_create = val_nil();
  nr_v_title = val_nil();
  nr_v_task_update = val_nil();
  nr_v_done = val_nil();
  nr_v_task_delete = val_nil();
  nr_v_api_main = val_nil();
  nr_v_app = val_nil();
  nr_v_api_get_tasks = val_nil();
  nr_v_api_get_task = val_nil();
  nr_v_t = val_nil();
  nr_v_api_create_task = val_nil();
  nr_v_api_update_task = val_nil();
  nr_v_api_delete_task = val_nil();
  nr_v_cli = val_nil();
  nr_v_cli_run = val_nil();
  nr_v_args = val_nil();
  nr_v_cmd = val_nil();
  nr_v_arg_offset = val_nil();
  nr_v_items = val_nil();
  nr_v_status = val_nil();
  nr_api_init();
  if (nr_v_api.type == VAL_NIL) nr_v_api = val_obj();
  set_field(nr_v_api, "main", val_func(nr_api_main));
  set_field(nr_v_api, "tasks", val_func(nr_api_get_tasks));
  set_field(nr_v_api, "task", val_func(nr_api_get_task));
  set_field(nr_v_api, "task", val_func(nr_api_create_task));
  set_field(nr_v_api, "task", val_func(nr_api_update_task));
  set_field(nr_v_api, "task", val_func(nr_api_delete_task));
;
  nr_cli_init();
  if (nr_v_cli.type == VAL_NIL) nr_v_cli = val_obj();
  set_field(nr_v_cli, "run", val_func(nr_cli_run));
;
  nr_sys_init();
  if (nr_v_sys.type == VAL_NIL) nr_v_sys = val_obj();
  set_field(nr_v_sys, "args", val_func(nr_sys_args));
  set_field(nr_v_sys, "now", val_func(nr_sys_now));
  set_field(nr_v_sys, "exit", val_func(nr_sys_exit));
;
  nr_v_args = ({ Value _t = nr_v_sys; Value _f = get_field(_t, "args"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_sys_args(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
;
  if (is_truthy(val_int(nr_rt_len(nr_v_args).data.i > val_int(1).data.i))) {
({ Value _t = nr_v_cli; Value _f = get_field(_t, "run"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, nr_v_args, val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_cli_run(_t, nr_v_args, val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  } else {
({ Value _t = nr_v_api; Value _f = get_field(_t, "main"); Value _r; if (_f.type == VAL_FUNC) _r = ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); else { _r = nr_api_main(_t, val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); } _r; });
  }
;

  ArenaBlock* curr = nr_arena->blocks; while(curr) { ArenaBlock* next = curr->next; free(curr->ptr); free(curr); curr = next; } free(nr_arena);
  return 0; 
}
