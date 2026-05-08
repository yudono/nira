#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
int nr_argc; char** nr_argv;
typedef enum { VAL_NIL, VAL_INT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_FLOAT, VAL_ERROR } ValueType;
typedef struct { char* heap_start; char* heap_end; char* current; } Arena; Arena* nr_arena;
void* nr_alloc(size_t sz) { sz = (sz + 7) & ~7; void* p = nr_arena->current; nr_arena->current += sz; return p; }
struct Value; typedef struct Value { ValueType type; int length; union { long long i; double f; char* s; void* func_ptr; struct { struct Value* elements; int count; int capacity; }* arr; struct { char** keys; struct Value* values; int count; int capacity; }* obj; } data; } Value;
#define val_nil() ((Value){.type = VAL_NIL})
#define val_int(v) ((Value){.type = VAL_INT, .data.i = (long long)(v)})
#define val_bool(b) ((Value){.type = VAL_BOOL, .data.i = (long long)(b)})
#define val_str_len(str, len) ((Value){.type = VAL_STR, .length = (len), .data.s = (char*)(str)})
#define val_str(str) val_str_len(str, strlen(str))
#define val_func(ptr) ((Value){.type = VAL_FUNC, .data.func_ptr = (void*)(ptr)})
#define IS_TRUTHY(v) ((v).type == VAL_BOOL ? (v).data.i : ((v).type != VAL_NIL))
Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 16; v.data.obj->keys = nr_alloc(sizeof(char*)*16); v.data.obj->values = nr_alloc(sizeof(Value)*16); return v; }
Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 16; v.data.arr->elements = nr_alloc(sizeof(Value)*16); return v; }
void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) { obj.data.obj->values[i] = val; return; } obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }
Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }
Value nr_rt_at(Value obj, Value idx) {
  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) return obj.data.arr->elements[i]; }
  if (obj.type == VAL_OBJ && idx.type == VAL_STR) return get_field(obj, idx.data.s);
  return val_nil();
}
void nr_rt_set_at(Value obj, Value idx, Value val) {
  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) obj.data.arr->elements[i] = val; }
  else if (obj.type == VAL_OBJ && idx.type == VAL_STR) set_field(obj, idx.data.s, val);
}
void nr_rt_print(Value v) {
  if (v.type == VAL_INT) printf("%lld", v.data.i);
  else if (v.type == VAL_FLOAT) printf("%g", v.data.f);
  else if (v.type == VAL_STR) printf("%s", v.data.s);
  else if (v.type == VAL_BOOL) printf("%s", v.data.i ? "true" : "false");
  else if (v.type == VAL_OBJ) printf("[Object]");
  else if (v.type == VAL_ARR) printf("[Array]");
  else if (v.type == VAL_ERROR) printf("Error: %s", v.data.s);
  else printf("nil");
}
Value nr_rt_len(Value v) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }
void nr_rt_push(Value arr, Value val) { if(arr.type!=VAL_ARR) return; if(arr.data.arr->count >= arr.data.arr->capacity) { int new_cap = arr.data.arr->capacity * 2; Value* new_elements = malloc(sizeof(Value) * new_cap); memcpy(new_elements, arr.data.arr->elements, sizeof(Value) * arr.data.arr->count); arr.data.arr->elements = new_elements; arr.data.arr->capacity = new_cap; } arr.data.arr->elements[arr.data.arr->count++] = val; }
Value nr_rt_typeof(Value v) {
  if (v.type == VAL_INT) return val_str("int"); if (v.type == VAL_FLOAT) return val_str("float"); if (v.type == VAL_STR) return val_str("string");
  if (v.type == VAL_BOOL) return val_str("bool"); if (v.type == VAL_OBJ) return val_str("object"); if (v.type == VAL_ARR) return val_str("array");
  if (v.type == VAL_NIL) return val_str("nil"); return val_str("unknown");
}
Value nr_rt_obj_keys(Value v) { if(v.type!=VAL_OBJ) return val_arr(); Value k = val_arr(); for(int i=0; i<v.data.obj->count; i++) nr_rt_push(k, val_str(v.data.obj->keys[i])); return k; }
Value nr_rt_to_string(Value v) { char* b = nr_alloc(128); if(v.type==VAL_INT) sprintf(b, "%lld", v.data.i); else if(v.type==VAL_FLOAT) sprintf(b, "%g", v.data.f); else if(v.type==VAL_STR) return v; else if(v.type==VAL_BOOL) return val_str(v.data.i ? "true" : "false"); else sprintf(b, "nil"); return val_str(b); }
Value nr_rt_add(Value l, Value r) {
  if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i);
  if(l.type==VAL_FLOAT && r.type==VAL_FLOAT) return (Value){.type = VAL_FLOAT, .data.f = l.data.f + r.data.f};
  if(l.type==VAL_STR || r.type==VAL_STR) {
    Value sl = nr_rt_to_string(l); Value sr = nr_rt_to_string(r);
    char* b = nr_alloc(sl.length + sr.length + 1); memcpy(b, sl.data.s, sl.length); memcpy(b+sl.length, sr.data.s, sr.length); b[sl.length+sr.length]=0; return val_str_len(b, sl.length+sr.length);
  }
  return val_nil();
}
void nr_time_init() {} 
Value nr_rt_now() { return val_int(time(NULL)); }
Value nr_rt_millis() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
Value nr_rt_sqrt(Value v) { double d = (v.type == VAL_FLOAT) ? v.data.f : (double)v.data.i; return (Value){.type = VAL_FLOAT, .data.f = sqrt(d)}; }
Value nr_rt_random() { return val_int(rand()); }
Value nr_rt_to_int(Value v) { if(v.type==VAL_INT) return v; if(v.type==VAL_STR) return val_int(atoll(v.data.s)); return val_int(0); }
Value nr_rt_json_encode(Value v) {
  if(v.type==VAL_INT) { char* b=nr_alloc(64); sprintf(b, "%lld", v.data.i); return val_str(b); }
  if(v.type==VAL_STR) { char* b=nr_alloc(strlen(v.data.s)+3); sprintf(b, "\"%s\"", v.data.s); return val_str(b); }
  if(v.type==VAL_ARR) { char* b=nr_strdup("["); for(int i=0; i<v.data.arr->count; i++) { Value item = nr_rt_json_encode(v.data.arr->elements[i]); char* next=nr_alloc(strlen(b)+strlen(item.data.s)+3); sprintf(next, "%s%s%s", b, item.data.s, (i==v.data.arr->count-1)?"":" "); b=next; } char* f=nr_alloc(strlen(b)+2); sprintf(f, "%s]", b); return val_str(f); }
  return val_str("null");
}
Value nr_rt_file_read(Value path) { if(path.type!=VAL_STR) return val_nil(); FILE* f=fopen(path.data.s, "rb"); if(!f) return val_nil(); fseek(f, 0, SEEK_END); long s=ftell(f); rewind(f); char* b=nr_alloc(s+1); fread(b, 1, s, f); b[s]=0; fclose(f); return val_str(b); }
Value nr_rt_http_serve(Value port_v, Value handler) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0); int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(port_v.data.i);
  bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)); listen(server_fd, 3);
  while(1) {
    int s = accept(server_fd, NULL, NULL); char buf[4096]={0}; read(s, buf, 4096); char m[16], p[256]; sscanf(buf, "%s %s", m, p);
    Value req = val_obj(); set_field(req, "method", val_str(strdup(m))); set_field(req, "path", val_str(strdup(p)));
    char* body_ptr = strstr(buf, "\r\n\r\n"); if(body_ptr) set_field(req, "body", val_str(strdup(body_ptr+4)));
    Value res_obj = val_nil();
    if(handler.type==VAL_FUNC) res_obj = handler.data.func.decl->data.func_decl.body ? ((Value(*)(Value,Value))handler.data.func.decl->data.func_decl.name)(val_nil(), req) : val_nil();
    char* body="Not Found"; int status=404; if(res_obj.type==VAL_OBJ) { Value b=get_field(res_obj, "body"); if(b.type==VAL_STR) body=b.data.s; Value st=get_field(res_obj, "status"); if(st.type==VAL_INT) status=st.data.i; }
    char resp[8192]; sprintf(resp, "HTTP/1.1 %d OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n%s", status, strlen(body), body); write(s, resp, strlen(resp)); close(s);
  } return val_nil();
}
Value nr_rt_load_module(const char* name) {
  if (strcmp(name, "time") == 0) { Value m = val_obj(); set_field(m, "now", val_func(nr_rt_now)); set_field(m, "millis", val_func(nr_rt_millis)); return m; }
  if (strcmp(name, "math") == 0) { Value m = val_obj(); set_field(m, "sqrt", val_func(nr_rt_sqrt)); set_field(m, "random", val_func(nr_rt_random)); return m; }
  return val_obj();
}
Value nr_handle_index(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_handle_get_tasks(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_handle_create_task(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_handle_delete_task(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_v_start;
Value nr_v_end;
Value nr_v_sum;
long long nr_v_i;
long long nr_v_j;
long long nr_v_n;
Value nr_v_temp;
char* nr_v_s; int nr_v_s_len;
Value nr_v_result;
Value nr_v_millis;
long long* nr_v_arr_unboxed; int nr_v_arr_count; Value nr_v_arr;
Value nr_v_count;
Value nr_v_http;
Value nr_v_file;
Value nr_v_json;
Value nr_v__tasks;
Value nr_v__next_id;
Value nr_v_handle_index;
Value nr_v_index_content;
Value nr_v_handle_get_tasks;
Value nr_v_handle_create_task;
Value nr_v_data;
Value nr_v_new_task;
Value nr_v_handle_delete_task;
Value nr_v_id_str;
Value nr_v_target_id;
Value nr_v_new_list;
Value nr_v_t;
Value nr_v_app;
Value nr_handle_index(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_req = _v0;
  Value nr_v_res = _v1;
({ nr_rt_print(val_str("GET / - Serving index.html")); val_nil(); });
  nr_v_index_content = ({ Value _f = get_field(nr_v_file, "read"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_str("examples/index.html"), val_nil(), val_nil(), val_nil(), val_nil()); });
({ Value _f = get_field(nr_v_res, "send"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), nr_v_index_content, val_nil(), val_nil(), val_nil(), val_nil()); });
  return val_nil();
}
Value nr_handle_get_tasks(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_req = _v0;
  Value nr_v_res = _v1;
({ nr_rt_print(val_str("GET /api/tasks")); val_nil(); });
({ Value _f = get_field(nr_v_res, "json"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), nr_v__tasks, val_nil(), val_nil(), val_nil(), val_nil()); });
  return val_nil();
}
Value nr_handle_create_task(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_req = _v0;
  Value nr_v_res = _v1;
({ nr_rt_print(nr_rt_add(val_str("POST /api/tasks - Body: "), nr_rt_at(nr_v_req, val_str("body")))); val_nil(); });
  nr_v_data = ({ Value _f = get_field(nr_v_json, "parse"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), nr_rt_at(nr_v_req, val_str("body")), val_nil(), val_nil(), val_nil(), val_nil()); });
  nr_v_new_task = ({ Value _o = val_obj(); set_field(_o, "id", nr_v__next_id); set_field(_o, "title", nr_rt_at(nr_v_data, val_str("title"))); set_field(_o, "done", val_bool(false)); _o; });
({ nr_rt_push(nr_v__tasks, nr_v_new_task); val_nil(); });
  nr_v__next_id = nr_rt_add(nr_v__next_id, val_int(1));
({ Value _f = get_field(nr_v_res, "json"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), nr_v_new_task, val_nil(), val_nil(), val_nil(), val_nil()); });
  return val_nil();
}
Value nr_handle_delete_task(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_req = _v0;
  Value nr_v_res = _v1;
  nr_v_id_str = nr_rt_at(nr_rt_at(nr_v_req, val_str("params")), val_str("id"));
({ nr_rt_print(nr_rt_add(val_str("DELETE /api/tasks/"), nr_v_id_str)); val_nil(); });
  nr_v_target_id = ({ Value _f = nr_v_toInt; ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), nr_v_id_str, val_nil(), val_nil(), val_nil(), val_nil()); });
  nr_v_new_list = ({ Value _a = val_arr(); _a; });
  { Value _iter = nr_v__tasks; if (_iter.type == VAL_ARR) {
    for (int _i = 0; _i < _iter.data.arr->count; _i++) {
      nr_v_t = _iter.data.arr->elements[_i];
  if (((nr_rt_at(nr_v_t, val_str("id"))).data.i != (nr_v_target_id).data.i)) {
({ nr_rt_push(nr_v_new_list, nr_v_t); val_nil(); });
  };
    }
  } }
;
  nr_v__tasks = nr_v_new_list;
({ Value _f = get_field(nr_v_res, "json"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), ({ Value _o = val_obj(); set_field(_o, "ok", val_bool(true)); _o; }), val_nil(), val_nil(), val_nil(), val_nil()); });
  return val_nil();
}
int main(int argc, char** argv) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  size_t heap_size = 1024 * 1024 * 1024; nr_arena = malloc(sizeof(Arena)); nr_arena->heap_start = malloc(heap_size); nr_arena->current = nr_arena->heap_start;
  nr_argc = argc; nr_argv = argv;
  nr_v_start = val_nil();
  nr_v_end = val_nil();
  nr_v_sum = val_nil();
  nr_v_i = 0;
  nr_v_j = 0;
  nr_v_n = 0;
  nr_v_temp = val_nil();
  nr_v_s = malloc(10 * 1024 * 1024); nr_v_s_len = 0;
  nr_v_result = val_nil();
  nr_v_millis = val_nil();
  nr_v_arr_unboxed = malloc(sizeof(long long) * 100000); nr_v_arr_count = 0; nr_v_arr = val_arr();
  nr_v_count = val_nil();
  nr_v_http = val_nil();
  nr_v_file = val_nil();
  nr_v_json = val_nil();
  nr_v__tasks = val_nil();
  nr_v__next_id = val_nil();
  nr_v_handle_index = val_nil();
  nr_v_index_content = val_nil();
  nr_v_handle_get_tasks = val_nil();
  nr_v_handle_create_task = val_nil();
  nr_v_data = val_nil();
  nr_v_new_task = val_nil();
  nr_v_handle_delete_task = val_nil();
  nr_v_id_str = val_nil();
  nr_v_target_id = val_nil();
  nr_v_new_list = val_nil();
  nr_v_t = val_nil();
  nr_v_app = val_nil();
  nr_v_http = val_obj(); set_field(nr_v_http, "listen", val_func(nr_rt_http_serve)); 
  nr_v_http = val_obj(); set_field(nr_v_http, "app", val_func(nr_rt_http_serve));
;
  nr_v_file = nr_rt_load_module("file");
  nr_v_file = val_obj(); set_field(nr_v_file, "read", val_func(nr_rt_file_read));
;
  nr_v_json = nr_rt_load_module("json");
  nr_v_json = val_obj(); set_field(nr_v_json, "parse", val_func(nr_rt_json_encode)); set_field(nr_v_json, "encode", val_func(nr_rt_json_encode));
;
  nr_v__tasks = ({ Value _a = val_arr(); nr_rt_push(_a, ({ Value _o = val_obj(); set_field(_o, "id", val_int(1)); set_field(_o, "title", val_str("Learn Nira Language")); set_field(_o, "done", val_bool(false)); _o; })); nr_rt_push(_a, ({ Value _o = val_obj(); set_field(_o, "id", val_int(2)); set_field(_o, "title", val_str("Master C Backend")); set_field(_o, "done", val_bool(true)); _o; })); nr_rt_push(_a, ({ Value _o = val_obj(); set_field(_o, "id", val_int(3)); set_field(_o, "title", val_str("Build Premium Web App")); set_field(_o, "done", val_bool(false)); _o; })); _a; });
  nr_v__next_id = val_int(4);
;
;
;
;
;
({ nr_rt_print(val_str("🚀 Nira Full Server Demo starting...")); val_nil(); });
  nr_v_app = ({ Value _f = get_field(nr_v_http, "app"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(val_str("App created")); val_nil(); });
({ Value _f = get_field(nr_v_app, "get"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_str("/"), val_func(nr_handle_index), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(val_str("Route / registered")); val_nil(); });
({ Value _f = get_field(nr_v_app, "get"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_str("/api/tasks"), val_func(nr_handle_get_tasks), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(val_str("Route /api/tasks registered")); val_nil(); });
({ Value _f = get_field(nr_v_app, "post"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_str("/api/tasks"), val_func(nr_handle_create_task), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(val_str("Route POST /api/tasks registered")); val_nil(); });
({ Value _f = get_field(nr_v_app, "delete"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_str("/api/tasks/:id"), val_func(nr_handle_delete_task), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(val_str("Route DELETE /api/tasks/:id registered")); val_nil(); });
({ nr_rt_print(val_str("✨ Server ready at http://localhost:3000")); val_nil(); });
({ Value _f = get_field(nr_v_app, "listen"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_int(3000), val_nil(), val_nil(), val_nil(), val_nil()); });
  return 0; 
}
