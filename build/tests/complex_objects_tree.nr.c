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
Value nr_create_employee(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_create_department(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_add_employee(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_add_sub_dept(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_print_org(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);
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
Value nr_v_create_employee;
Value nr_v_create_department;
Value nr_v_add_employee;
Value nr_v_emps;
Value nr_v_add_sub_dept;
Value nr_v_subs;
Value nr_v_print_org;
Value nr_v_emp;
Value nr_v_sub;
Value nr_v_head_office;
Value nr_v_tech;
Value nr_v_backend;
Value nr_v_frontend;
Value nr_v_hr;
Value nr_create_employee(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_id = _v0;
  Value nr_v_name = _v1;
  Value nr_v_role = _v2;
  return ({ Value _o = val_obj(); set_field(_o, "id", nr_v_id); set_field(_o, "name", nr_v_name); set_field(_o, "role", nr_v_role); set_field(_o, "projects", ({ Value _a = val_arr(); _a; })); _o; });
  return val_nil();
}
Value nr_create_department(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_name = _v0;
  return ({ Value _o = val_obj(); set_field(_o, "name", nr_v_name); set_field(_o, "employees", ({ Value _a = val_arr(); _a; })); set_field(_o, "sub_departments", ({ Value _a = val_arr(); _a; })); _o; });
  return val_nil();
}
Value nr_add_employee(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_dept = _v0;
  Value nr_v_emp = _v1;
  nr_v_emps = nr_rt_at(nr_v_dept, val_str("employees"));
({ nr_rt_push(nr_v_emps, nr_v_emp); val_nil(); });
  return val_nil();
}
Value nr_add_sub_dept(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_dept = _v0;
  Value nr_v_sub = _v1;
  nr_v_subs = nr_rt_at(nr_v_dept, val_str("sub_departments"));
({ nr_rt_push(nr_v_subs, nr_v_sub); val_nil(); });
  return val_nil();
}
Value nr_print_org(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;
  Value nr_v_dept = _v0;
  Value nr_v_indent = _v1;
({ nr_rt_print(nr_rt_add(nr_rt_add(nr_v_indent, val_str("Department: ")), nr_rt_to_string(nr_rt_at(nr_v_dept, val_str("name"))))); val_nil(); });
  { Value _iter = nr_rt_at(nr_v_dept, val_str("employees")); if (_iter.type == VAL_ARR) {
    for (int _i = 0; _i < _iter.data.arr->count; _i++) {
      nr_v_emp = _iter.data.arr->elements[_i];
({ nr_rt_print(nr_rt_add(nr_rt_add(nr_rt_add(nr_rt_add(nr_rt_add(nr_v_indent, val_str("  - Employee: ")), nr_rt_to_string(nr_rt_at(nr_v_emp, val_str("name")))), val_str(" (")), nr_rt_to_string(nr_rt_at(nr_v_emp, val_str("role")))), val_str(")"))); val_nil(); });
    }
  } }
;
  { Value _iter = nr_rt_at(nr_v_dept, val_str("sub_departments")); if (_iter.type == VAL_ARR) {
    for (int _i = 0; _i < _iter.data.arr->count; _i++) {
      nr_v_sub = _iter.data.arr->elements[_i];
nr_print_org(val_nil(), nr_v_sub, nr_rt_add(nr_v_indent, val_str("    ")), val_nil(), val_nil(), val_nil());
    }
  } }
;
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
  nr_v_create_employee = val_nil();
  nr_v_create_department = val_nil();
  nr_v_add_employee = val_nil();
  nr_v_emps = val_nil();
  nr_v_add_sub_dept = val_nil();
  nr_v_subs = val_nil();
  nr_v_print_org = val_nil();
  nr_v_emp = val_nil();
  nr_v_sub = val_nil();
  nr_v_head_office = val_nil();
  nr_v_tech = val_nil();
  nr_v_backend = val_nil();
  nr_v_frontend = val_nil();
  nr_v_hr = val_nil();
;
;
;
;
;
;
({ nr_rt_print(val_str("=== COMPANY ORG TEST ===")); val_nil(); });
  nr_v_head_office = nr_create_department(val_nil(), val_str("Head Office"), val_nil(), val_nil(), val_nil(), val_nil());
  nr_v_tech = nr_create_department(val_nil(), val_str("Technology"), val_nil(), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_tech, nr_create_employee(val_nil(), val_int(1), val_str("Alice"), val_str("CTO"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_tech, nr_create_employee(val_nil(), val_int(2), val_str("Bob"), val_str("Lead Engineer"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
  nr_v_backend = nr_create_department(val_nil(), val_str("Backend Team"), val_nil(), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_backend, nr_create_employee(val_nil(), val_int(3), val_str("Charlie"), val_str("Senior Dev"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_backend, nr_create_employee(val_nil(), val_int(4), val_str("David"), val_str("Junior Dev"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
nr_add_sub_dept(val_nil(), nr_v_tech, nr_v_backend, val_nil(), val_nil(), val_nil());
  nr_v_frontend = nr_create_department(val_nil(), val_str("Frontend Team"), val_nil(), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_frontend, nr_create_employee(val_nil(), val_int(5), val_str("Eve"), val_str("UI Designer"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
nr_add_sub_dept(val_nil(), nr_v_tech, nr_v_frontend, val_nil(), val_nil(), val_nil());
  nr_v_hr = nr_create_department(val_nil(), val_str("Human Resources"), val_nil(), val_nil(), val_nil(), val_nil());
nr_add_employee(val_nil(), nr_v_hr, nr_create_employee(val_nil(), val_int(6), val_str("Frank"), val_str("HR Manager"), val_nil(), val_nil()), val_nil(), val_nil(), val_nil());
nr_add_sub_dept(val_nil(), nr_v_head_office, nr_v_tech, val_nil(), val_nil(), val_nil());
nr_add_sub_dept(val_nil(), nr_v_head_office, nr_v_hr, val_nil(), val_nil(), val_nil());
nr_print_org(val_nil(), nr_v_head_office, val_str(""), val_nil(), val_nil(), val_nil());
({ nr_rt_print(val_str("=== ORG TEST PASSED ===")); val_nil(); });
  return 0; 
}
