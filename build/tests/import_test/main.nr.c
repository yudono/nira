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
Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 8; v.data.obj->keys = nr_alloc(sizeof(char*)*8); v.data.obj->values = nr_alloc(sizeof(Value)*8); return v; }
Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 8; v.data.arr->elements = nr_alloc(sizeof(Value)*8); return v; }
void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }
Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }
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
Value nr_rt_to_string(Value v) { char* b = nr_alloc(64); if(v.type==VAL_INT) sprintf(b, "%lld", v.data.i); else if(v.type==VAL_FLOAT) sprintf(b, "%g", v.data.f); else if(v.type==VAL_STR) return v; else sprintf(b, "nil"); return val_str(b); }
Value nr_rt_add(Value l, Value r) { if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i); if(l.type==VAL_STR && r.type==VAL_STR) { char* b = nr_alloc(l.length + r.length + 1); memcpy(b, l.data.s, l.length); memcpy(b+l.length, r.data.s, r.length); b[l.length+r.length]=0; return val_str_len(b, l.length+r.length); } return val_nil(); }
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
long long nr_v_start;
long long nr_v_end;
long long nr_v_sum;
long long nr_v_i;
long long nr_v_j;
long long nr_v_n;
long long nr_v_temp;
char* nr_v_s; int nr_v_s_len;
long long nr_v_result;
long long nr_v_millis;
long long* nr_v_arr_unboxed; int nr_v_arr_count; Value nr_v_arr;
long long nr_v_count;
Value nr_v_config;
Value nr_v_time;
Value nr_v_math;
Value nr_v_sqrt;
Value nr_v_a;
Value nr_v_b;
Value nr_v_c;
Value nr_v_now;
Value nr_v_res;
int main(int argc, char** argv) {
  size_t heap_size = 1024 * 1024 * 1024; nr_arena = malloc(sizeof(Arena)); nr_arena->heap_start = malloc(heap_size); nr_arena->current = nr_arena->heap_start;
  nr_argc = argc; nr_argv = argv;
  nr_v_start = 0;
  nr_v_end = 0;
  nr_v_sum = 0;
  nr_v_i = 0;
  nr_v_j = 0;
  nr_v_n = 0;
  nr_v_temp = 0;
  nr_v_s = malloc(10 * 1024 * 1024); nr_v_s_len = 0;
  nr_v_result = 0;
  nr_v_millis = 0;
  nr_v_arr_unboxed = malloc(sizeof(long long) * 100000); nr_v_arr_count = 0; nr_v_arr = val_arr();
  nr_v_count = 0;
  nr_v_config = val_nil();
  nr_v_time = val_nil();
  nr_v_math = val_nil();
  nr_v_sqrt = val_nil();
  nr_v_a = val_nil();
  nr_v_b = val_nil();
  nr_v_c = val_nil();
  nr_v_now = val_nil();
  nr_v_res = val_nil();
  nr_v_config = nr_rt_load_module("config/config");
  set_field(nr_v_config, "db", val_str("sqlite"));
;
  nr_v_time = nr_rt_load_module("time");
;
  nr_v_math = nr_rt_load_module("math");
;
  { Value _m = nr_rt_load_module("math"); nr_v_sqrt = get_field(_m, "sqrt"); }
;
;
  nr_v_a = val_int(10);
  nr_v_a = val_str("20");
  nr_v_b = val_int(30);
  nr_v_c = nr_rt_add(nr_v_a, nr_v_b);
({ nr_rt_print(nr_rt_add(val_str("c: "), nr_rt_to_string(nr_v_c))); printf("\n"); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Config DB: "), get_field(nr_v_config, "db"))); printf("\n"); val_nil(); });
  nr_v_now = ({ Value _f = get_field(nr_v_time, "now"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(nr_rt_add(val_str("Current time: "), nr_rt_to_string(nr_v_now))); printf("\n"); val_nil(); });
  nr_v_res = ({ Value _f = nr_v_sqrt; ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_int(16), val_nil(), val_nil(), val_nil(), val_nil()); });
({ nr_rt_print(nr_rt_add(val_str("Sqrt(16): "), nr_rt_to_string(nr_v_res))); printf("\n"); val_nil(); });
({ nr_rt_print(nr_rt_add(val_str("Random: "), nr_rt_to_string(({ Value _f = get_field(nr_v_math, "random"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil()); })))); printf("\n"); val_nil(); });
  return 0; 
}
