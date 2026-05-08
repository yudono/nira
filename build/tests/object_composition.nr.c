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
Value nr_rt_load_module(const char* name) {
  if (strcmp(name, "time") == 0) { Value m = val_obj(); set_field(m, "now", val_func(nr_rt_now)); set_field(m, "millis", val_func(nr_rt_millis)); return m; }
  if (strcmp(name, "math") == 0) { Value m = val_obj(); set_field(m, "sqrt", val_func(nr_rt_sqrt)); set_field(m, "random", val_func(nr_rt_random)); return m; }
  return val_obj();
}
Value nr_create_point(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_create_size(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_create_rect(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_get_area(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_move_rect(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_contains_point(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
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
Value nr_v_create_point;
Value nr_v_create_size;
Value nr_v_create_rect;
Value nr_v_get_area;
Value nr_v_move_rect;
Value nr_v_contains_point;
Value nr_v_r1;
Value nr_v_p1;
Value nr_v_p2;
Value nr_v_scene;
Value nr_create_point(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_x = _v0;
  Value nr_v_y = _v1;
  return ({ Value _o = val_obj(); set_field(_o, "x", nr_v_x); set_field(_o, "y", nr_v_y); _o; });
  return val_nil();
}
Value nr_create_size(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_w = _v0;
  Value nr_v_h = _v1;
  return ({ Value _o = val_obj(); set_field(_o, "w", nr_v_w); set_field(_o, "h", nr_v_h); _o; });
  return val_nil();
}
Value nr_create_rect(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_x = _v0;
  Value nr_v_y = _v1;
  Value nr_v_w = _v2;
  Value nr_v_h = _v3;
  return ({ Value _o = val_obj(); set_field(_o, "origin", nr_create_point(val_nil(), nr_v_x, nr_v_y, val_nil(), val_nil(), val_nil())); set_field(_o, "size", nr_create_size(val_nil(), nr_v_w, nr_v_h, val_nil(), val_nil(), val_nil())); _o; });
  return val_nil();
}
Value nr_get_area(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_rect = _v0;
  return val_int(((nr_rt_at(nr_rt_at(nr_v_rect, val_str("size")), val_str("w"))).data.i * (nr_rt_at(nr_rt_at(nr_v_rect, val_str("size")), val_str("h"))).data.i));
  return val_nil();
}
Value nr_move_rect(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_rect = _v0;
  Value nr_v_dx = _v1;
  Value nr_v_dy = _v2;
  nr_rt_set_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("x"), nr_rt_add(nr_rt_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("x")), nr_v_dx));
  nr_rt_set_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("y"), nr_rt_add(nr_rt_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("y")), nr_v_dy));
  return val_nil();
}
Value nr_contains_point(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_rect = _v0;
  Value nr_v_p = _v1;
  if (((nr_rt_at(nr_v_p, val_str("x"))).data.i < (nr_rt_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("x"))).data.i)) {
  return val_bool(false);
  };
  { long long* _e1 = &nr_v_arr_unboxed[nr_v_j]; long long* _e2 = &nr_v_arr_unboxed[nr_v_j + 1]; long long _v1 = *_e1; long long _v2 = *_e2; int _c = _v1 > _v2; *_e1 = _c ? _v2 : _v1; *_e2 = _c ? _v1 : _v2; };
  if (((nr_rt_at(nr_v_p, val_str("y"))).data.i < (nr_rt_at(nr_rt_at(nr_v_rect, val_str("origin")), val_str("y"))).data.i)) {
  return val_bool(false);
  };
  { long long* _e1 = &nr_v_arr_unboxed[nr_v_j]; long long* _e2 = &nr_v_arr_unboxed[nr_v_j + 1]; long long _v1 = *_e1; long long _v2 = *_e2; int _c = _v1 > _v2; *_e1 = _c ? _v2 : _v1; *_e2 = _c ? _v1 : _v2; };
  return val_bool(true);
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
  nr_v_create_point = val_nil();
  nr_v_create_size = val_nil();
  nr_v_create_rect = val_nil();
  nr_v_get_area = val_nil();
  nr_v_move_rect = val_nil();
  nr_v_contains_point = val_nil();
  nr_v_r1 = val_nil();
  nr_v_p1 = val_nil();
  nr_v_p2 = val_nil();
  nr_v_scene = val_nil();
;
;
;
;
;
;
;
({ nr_rt_print(val_str("=== OBJECT COMPOSITION TEST ===")); val_nil(); });
  nr_v_r1 = nr_create_rect(val_nil(), val_int(10), val_int(10), val_int(50), val_int(50), val_nil());
({ nr_rt_print(nr_rt_add(val_str("Rect 1 Area: "), nr_rt_to_string(nr_get_area(val_nil(), nr_v_r1, val_nil(), val_nil(), val_nil(), val_nil())))); val_nil(); });
  nr_v_p1 = nr_create_point(val_nil(), val_int(20), val_int(20), val_nil(), val_nil(), val_nil());
  nr_v_p2 = nr_create_point(val_nil(), val_int(5), val_int(5), val_nil(), val_nil(), val_nil());
({ nr_rt_print(nr_rt_add(val_str("Contains P1 (20,20): "), nr_rt_to_string(nr_contains_point(val_nil(), nr_v_r1, nr_v_p1, val_nil(), val_nil(), val_nil())))); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Contains P2 (5,5): "), nr_rt_to_string(nr_contains_point(val_nil(), nr_v_r1, nr_v_p2, val_nil(), val_nil(), val_nil())))); val_nil(); });
({ nr_rt_print(val_str("Moving rect by (20, 20)...")); val_nil(); });
nr_move_rect(val_nil(), nr_v_r1, val_int(20), val_int(20), val_nil(), val_nil());
({ nr_rt_print(nr_rt_add(nr_rt_add(nr_rt_add(nr_rt_add(val_str("New Origin: ("), nr_rt_to_string(nr_rt_at(nr_rt_at(nr_v_r1, val_str("origin")), val_str("x")))), val_str(", ")), nr_rt_to_string(nr_rt_at(nr_rt_at(nr_v_r1, val_str("origin")), val_str("y")))), val_str(")"))); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Contains P1 (20,20) after move: "), nr_rt_to_string(nr_contains_point(val_nil(), nr_v_r1, nr_v_p1, val_nil(), val_nil(), val_nil())))); val_nil(); });
  nr_v_scene = ({ Value _o = val_obj(); set_field(_o, "name", val_str("Main Scene")); set_field(_o, "shapes", ({ Value _a = val_arr(); nr_rt_push(_a, nr_v_r1); nr_rt_push(_a, nr_create_rect(val_nil(), val_int(0), val_int(0), val_int(100), val_int(100), val_nil())); _a; })); set_field(_o, "metadata", ({ Value _o = val_obj(); set_field(_o, "version", ((Value){.type = VAL_FLOAT, .data.f = 1})); set_field(_o, "tags", ({ Value _a = val_arr(); nr_rt_push(_a, val_str("vector")); nr_rt_push(_a, val_str("graphics")); _a; })); _o; })); _o; });
({ nr_rt_print(nr_rt_add(val_str("Scene: "), nr_rt_to_string(nr_rt_at(nr_v_scene, val_str("name"))))); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Shapes count: "), nr_rt_to_string(nr_rt_len(nr_rt_at(nr_v_scene, val_str("shapes")))))); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("First shape area: "), nr_rt_to_string(nr_get_area(val_nil(), nr_rt_at(nr_rt_at(nr_v_scene, val_str("shapes")), val_int(0)), val_nil(), val_nil(), val_nil(), val_nil())))); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Metadata Version: "), nr_rt_to_string(nr_rt_at(nr_rt_at(nr_v_scene, val_str("metadata")), val_str("version"))))); val_nil(); });
({ nr_rt_print(val_str("=== COMPOSITION TEST PASSED ===")); val_nil(); });
  return 0; 
}
