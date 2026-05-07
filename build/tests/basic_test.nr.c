#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sqlite3.h>
#include <regex.h>
#include <ctype.h>
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
char* val_to_json(Value v) { if (v.type == VAL_INT) { char* b = nr_alloc(32); sprintf(b, "%d", v.data.i); return b; } if (v.type == VAL_STR) { char* b = nr_alloc(strlen(v.data.s) + 3); sprintf(b, "\"%s\"", v.data.s); return b; } if (v.type == VAL_BOOL) return nr_strdup(v.data.i ? "true" : "false"); if (v.type == VAL_ARR) { char* res = nr_strdup("["); for (int i=0; i<v.data.arr->count; i++) { char* item = val_to_json(*v.data.arr->elements[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(item) + 3); sprintf(res, "%s%s%s", old, item, i < v.data.arr->count-1 ? "," : ""); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, "%s]", old); return res; } if (v.type == VAL_OBJ) { char* res = nr_strdup("{"); for (int i=0; i<v.data.obj->count; i++) { char* val = val_to_json(*v.data.obj->values[i]); char* old = res; res = nr_alloc(strlen(old) + strlen(v.data.obj->keys[i]) + strlen(val) + 6); sprintf(res, "%s\"%s\":%s%s", old, v.data.obj->keys[i], val, i < v.data.obj->count-1 ? "," : ""); } char* old = res; res = nr_alloc(strlen(old) + 2); sprintf(res, "%s}", old); return res; } return nr_strdup("null"); }
Value nr_rt_json_encode(Value v) { return val_str(val_to_json(v)); }

Value nr_rt_db_open(Value path) { return path; }
void bind_val(sqlite3_stmt* stmt, int idx, Value v) {
  if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, idx, v.data.i);
  else if (v.type == VAL_STR) sqlite3_bind_text(stmt, idx, v.data.s, -1, SQLITE_TRANSIENT);
  else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, idx);
}
Value nr_rt_db_exec(Value db, Value sql, Value params) {
  if (sql.type != VAL_STR || db.type != VAL_STR) return val_int(0);
  sqlite3* h; if (sqlite3_open(db.data.s, &h) != SQLITE_OK) return val_int(0);
  sqlite3_stmt* stmt; if (sqlite3_prepare_v2(h, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
    if (params.type == VAL_ARR) { for (int i=0; i<params.data.arr->count; i++) bind_val(stmt, i+1, *params.data.arr->elements[i]); }
    sqlite3_step(stmt); sqlite3_finalize(stmt);
  }
  sqlite3_close(h); return val_int(1);
}
Value nr_rt_db_query(Value db, Value sql, Value params) {
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
        else v = val_str((char*)sqlite3_column_text(stmt, i));
        set_field(o, name, v);
      }
      nr_rt_push(a, o);
    }
    sqlite3_finalize(stmt);
  }
  sqlite3_close(h); return a;
}
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
Value nr_v_a;
Value nr_v_b;
Value nr_v_s;


int main(int argc, char** argv) {
  nr_arena = malloc(sizeof(Arena)); nr_arena->blocks = NULL;
  srand(time(NULL));
  nr_argc = argc; nr_argv = argv;
  Value self = val_nil(); (void)self;
  nr_v_a = val_nil();
  nr_v_b = val_nil();
  nr_v_s = val_nil();
;
nr_rt_print(val_str("--- Basic Test ---"));
  nr_v_a = val_int(10);
  nr_v_b = val_int(20);
nr_rt_print(nr_rt_add(nr_v_a, nr_v_b));
  nr_v_s = val_str("Hello");
nr_rt_print(nr_rt_add(nr_v_s, val_str(" World")));
  if (is_truthy(val_int(nr_v_a.data.i < nr_v_b.data.i))) {
nr_rt_print(val_str("a < b"));
  };

  ArenaBlock* curr = nr_arena->blocks; while(curr) { ArenaBlock* next = curr->next; free(curr->ptr); free(curr); curr = next; } free(nr_arena);
  return 0; 
}
