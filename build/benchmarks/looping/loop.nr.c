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
#define IS_TRUTHY(v) ((v).type == VAL_BOOL ? (v).data.i : ((v).type != VAL_NIL))
Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 8; v.data.obj->keys = nr_alloc(sizeof(char*)*8); v.data.obj->values = nr_alloc(sizeof(Value)*8); return v; }
Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 8; v.data.arr->elements = nr_alloc(sizeof(Value)*8); return v; }
void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }
Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }
void nr_rt_print(Value v) { if (v.type == VAL_INT) printf("%lld\n", v.data.i); else if (v.type == VAL_STR) printf("%s\n", v.data.s); else if (v.type == VAL_BOOL) printf("%s\n", v.data.i ? "true" : "false"); else printf("nil\n"); }
Value nr_rt_len(Value v) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }
Value nr_rt_to_string(Value v) { char* b = nr_alloc(64); if(v.type==VAL_INT) sprintf(b, "%lld", v.data.i); else if(v.type==VAL_STR) return v; else sprintf(b, "nil"); return val_str(b); }
Value nr_rt_add(Value l, Value r) { if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i); if(l.type==VAL_STR && r.type==VAL_STR) { char* b = nr_alloc(l.length + r.length + 1); memcpy(b, l.data.s, l.length); memcpy(b+l.length, r.data.s, r.length); b[l.length+r.length]=0; return val_str_len(b, l.length+r.length); } return val_nil(); }
void nr_time_init() {} 
Value nr_rt_now() { return val_int(time(NULL)); }
Value nr_rt_millis() { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
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
;
;
  nr_v_start = (nr_rt_millis()).data.i;
  nr_v_sum = 0LL;
  nr_v_i = 0LL;
  while ((nr_v_i < 100000000LL)) {
  nr_v_sum = (nr_v_sum + nr_v_i);
  nr_v_i = (nr_v_i + 1LL);
  }
;
  nr_v_end = (nr_rt_millis()).data.i;
nr_rt_print(nr_rt_add(nr_rt_add(nr_rt_add(nr_rt_add(val_str("Nira: "), nr_rt_to_string(val_int((nr_v_end - nr_v_start)))), val_str(" ms (Result: ")), nr_rt_to_string(val_int(nr_v_sum))), val_str(")")));
  return 0; 
}
