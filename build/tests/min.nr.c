#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sqlite3.h>
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
char* nr_strdup(const char* s) { int l=strlen(s); char* d=nr_alloc(l+1); strcpy(d,s); return d; }
void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) { obj.data.obj->values[i] = val; return; } obj.data.obj->keys[obj.data.obj->count] = nr_strdup(key); obj.data.obj->values[obj.data.obj->count++] = val; }
Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }
Value nr_rt_at(Value self, Value obj, Value idx, Value _v2, Value _v3, Value _v4) {
  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) return obj.data.arr->elements[i]; }
  if (obj.type == VAL_OBJ && idx.type == VAL_STR) return get_field(obj, idx.data.s);
  return val_nil();
}
Value nr_rt_set_at(Value self, Value obj, Value idx, Value val, Value _v3, Value _v4) {
  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) obj.data.arr->elements[i] = val; }
  else if (obj.type == VAL_OBJ && idx.type == VAL_STR) set_field(obj, idx.data.s, val);
  return val_nil();
}
Value nr_rt_print(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {
  if (v.type == VAL_INT) printf("%lld", v.data.i);
  else if (v.type == VAL_FLOAT) printf("%g", v.data.f);
  else if (v.type == VAL_STR) printf("%s", v.data.s);
  else if (v.type == VAL_BOOL) printf("%s", v.data.i ? "true" : "false");
  else if (v.type == VAL_OBJ) printf("[Object]");
  else if (v.type == VAL_ARR) printf("[Array]");
  else if (v.type == VAL_ERROR) printf("Error: %s", v.data.s);
  else printf("nil");
  printf("\n"); fflush(stdout); return val_nil();
}
Value nr_rt_len(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }
Value nr_rt_push(Value self, Value arr, Value val, Value _v2, Value _v3, Value _v4) { if(arr.type!=VAL_ARR) return val_nil(); if(arr.data.arr->count >= arr.data.arr->capacity) { int new_cap = arr.data.arr->capacity * 2; Value* new_elements = malloc(sizeof(Value) * new_cap); memcpy(new_elements, arr.data.arr->elements, sizeof(Value) * arr.data.arr->count); arr.data.arr->elements = new_elements; arr.data.arr->capacity = new_cap; } arr.data.arr->elements[arr.data.arr->count++] = val; return val_nil(); }
Value nr_rt_typeof(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {
  if (v.type == VAL_INT) return val_str("int"); if (v.type == VAL_FLOAT) return val_str("float"); if (v.type == VAL_STR) return val_str("string");
  if (v.type == VAL_BOOL) return val_str("bool"); if (v.type == VAL_OBJ) return val_str("object"); if (v.type == VAL_ARR) return val_str("array");
  if (v.type == VAL_NIL) return val_str("nil"); return val_str("unknown");
}
Value nr_rt_obj_keys(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type!=VAL_OBJ) return val_arr(); Value k = val_arr(); for(int i=0; i<v.data.obj->count; i++) nr_rt_push(val_nil(), k, val_str(v.data.obj->keys[i]), val_nil(), val_nil(), val_nil()); return k; }
Value nr_rt_to_string(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { char* b = nr_alloc(128); if(v.type==VAL_INT) sprintf(b, "%lld", v.data.i); else if(v.type==VAL_FLOAT) sprintf(b, "%g", v.data.f); else if(v.type==VAL_STR) return v; else if(v.type==VAL_BOOL) return val_str(v.data.i ? "true" : "false"); else sprintf(b, "nil"); return val_str(b); }
Value nr_rt_add(Value l, Value r) {
  if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i);
  if(l.type==VAL_FLOAT && r.type==VAL_FLOAT) return (Value){.type = VAL_FLOAT, .data.f = l.data.f + r.data.f};
  if(l.type==VAL_STR || r.type==VAL_STR) {
    Value sl = nr_rt_to_string(val_nil(), l, val_nil(), val_nil(), val_nil(), val_nil()); Value sr = nr_rt_to_string(val_nil(), r, val_nil(), val_nil(), val_nil(), val_nil());
    char* b = nr_alloc(sl.length + sr.length + 1); memcpy(b, sl.data.s, sl.length); memcpy(b+sl.length, sr.data.s, sr.length); b[sl.length+sr.length]=0; return val_str_len(b, sl.length+sr.length);
  }
  return val_nil();
}
void nr_time_init() {} 
Value nr_rt_now(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(time(NULL)); }
Value nr_rt_millis(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }
Value nr_rt_sqrt(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { double d = (v.type == VAL_FLOAT) ? v.data.f : (double)v.data.i; return (Value){.type = VAL_FLOAT, .data.f = sqrt(d)}; }
Value nr_rt_random(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(rand()); }
Value nr_rt_to_int(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_INT) return v; if(v.type==VAL_STR) return val_int(atoll(v.data.s)); return val_int(0); }
Value nr_rt_json_encode(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {
  if(v.type==VAL_INT) { char* b=nr_alloc(64); sprintf(b, "%lld", v.data.i); return val_str(b); }
  if(v.type==VAL_FLOAT) { char* b=nr_alloc(64); sprintf(b, "%g", v.data.f); return val_str(b); }
  if(v.type==VAL_BOOL) return val_str(v.data.i ? "true" : "false");
  if(v.type==VAL_NIL) return val_str("null");
  if(v.type==VAL_STR) { char* b=nr_alloc(strlen(v.data.s)+3); sprintf(b, "\"%s\"", v.data.s); return val_str(b); }
  if(v.type==VAL_ARR) { char* b=nr_strdup("["); for(int i=0; i<v.data.arr->count; i++) { Value item=nr_rt_json_encode(val_nil(), v.data.arr->elements[i], val_nil(), val_nil(), val_nil(), val_nil()); char* n2=nr_alloc(strlen(b)+strlen(item.data.s)+3); sprintf(n2, "%s%s%s", b, item.data.s, (i==v.data.arr->count-1)?"":","); b=n2; } char* f=nr_alloc(strlen(b)+2); sprintf(f, "%s]", b); return val_str(f); }
  if(v.type==VAL_OBJ) { char* b=nr_strdup("{"); for(int i=0; i<v.data.obj->count; i++) { Value vl=nr_rt_json_encode(val_nil(), v.data.obj->values[i], val_nil(), val_nil(), val_nil(), val_nil()); char* n2=nr_alloc(strlen(b)+strlen(v.data.obj->keys[i])+strlen(vl.data.s)+6); sprintf(n2, "%s\"%s\":%s%s", b, v.data.obj->keys[i], vl.data.s, (i==v.data.obj->count-1)?"":","); b=n2; } char* f=nr_alloc(strlen(b)+2); sprintf(f, "%s}", b); return val_str(f); }
  return val_str("null");
}
Value nr_rt_json_parse_internal(char** p) {
    while(**p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t')) (*p)++;
    if (**p == '{') {
        (*p)++; Value o = val_obj();
        while (**p && **p != '}') {
            while(**p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t' || **p == ',')) (*p)++;
            if (**p == '}') break;
            if (**p == '\"') {
                (*p)++; char* start = *p; while(**p && **p != '\"') (*p)++;
                int len = *p - start; char* key = nr_alloc(len + 1); memcpy(key, start, len); key[len] = 0; (*p)++;
                while(**p && (**p == ' ' || **p == ':')) (*p)++;
                Value val = nr_rt_json_parse_internal(p);
                set_field(o, key, val);
            } else (*p)++;
        }
        if (**p == '}') (*p)++; return o;
    }
    if (**p == '\"') {
        (*p)++; char* start = *p; while(**p && **p != '\"') (*p)++;
        int len = *p - start; char* s = nr_alloc(len + 1); memcpy(s, start, len); s[len] = 0; (*p)++;
        return val_str(s);
    }
    if (**p == '[' ) {
        (*p)++; Value a = val_arr();
        while (**p && **p != ']') {
            while(**p && (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t' || **p == ',')) (*p)++;
            if (**p == ']') break;
            nr_rt_push(val_nil(), a, nr_rt_json_parse_internal(p), val_nil(), val_nil(), val_nil());
        }
        if (**p == ']') (*p)++; return a;
    }
    if ((**p >= '0' && **p <= '9') || **p == '-') {
        char* start = *p; while(**p && ((**p >= '0' && **p <= '9') || **p == '.' || **p == '-')) (*p)++;
        char buf[64]; int len = *p - start; if(len > 63) len = 63; memcpy(buf, start, len); buf[len] = 0;
        if (strchr(buf, '.')) return (Value){.type = VAL_FLOAT, .data.f = atof(buf)};
        return val_int(atoll(buf));
    }
    if (strncmp(*p, "true", 4) == 0) { *p += 4; return val_bool(true); }
    if (strncmp(*p, "false", 5) == 0) { *p += 5; return val_bool(false); }
    if (strncmp(*p, "null", 4) == 0) { *p += 4; return val_nil(); }
    return val_nil();
}
Value nr_rt_json_parse(Value self, Value s, Value _v1, Value _v2, Value _v3, Value _v4) { if(s.type!=VAL_STR) return val_nil(); char* p = s.data.s; return nr_rt_json_parse_internal(&p); }
Value nr_rt_file_read(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR) return val_nil(); FILE* f=fopen(path.data.s, "rb"); if(!f) return val_nil(); fseek(f, 0, SEEK_END); long s=ftell(f); rewind(f); char* b=nr_alloc(s+1); fread(b, 1, s, f); b[s]=0; fclose(f); return val_str(b); }
Value nr_rt_file_write(Value self, Value path, Value content, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR || content.type!=VAL_STR) return val_bool(0); FILE* f=fopen(path.data.s, "wb"); if(!f) return val_bool(0); fwrite(content.data.s, 1, strlen(content.data.s), f); fclose(f); return val_bool(1); }
Value nr_rt_file_delete(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR) return val_bool(0); return val_bool(remove(path.data.s)==0); }
Value nr_rt_file_exists(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR) return val_bool(0); return val_bool(access(path.data.s, 0)==0); }
Value nr_rt_sys_run(Value self, Value cmd, Value _v1, Value _v2, Value _v3, Value _v4) { if(cmd.type!=VAL_STR) return val_nil(); return val_int(system(cmd.data.s)); }
Value nr_rt_file_append(Value self, Value path, Value content, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR || content.type!=VAL_STR) return val_bool(0); FILE* f=fopen(path.data.s, "ab"); if(!f) return val_bool(0); fwrite(content.data.s, 1, strlen(content.data.s), f); fclose(f); return val_bool(1); }
Value nr_rt_time_to_unix(Value self, Value obj, Value _v1, Value _v2, Value _v3, Value _v4) { if(obj.type!=VAL_OBJ) return val_int(0); struct tm t={0}; t.tm_year=(int)get_field(obj,"year").data.i-1900; t.tm_mon=(int)get_field(obj,"month").data.i-1; t.tm_mday=(int)get_field(obj,"day").data.i; return val_int((long long)mktime(&t)); }
Value nr_rt_from_date(Value self, Value y, Value m, Value d, Value _v3, Value _v4) { Value o = val_obj(); set_field(o, "year", y); set_field(o, "month", m); set_field(o, "day", d); return o; }
Value nr_rt_delay(Value self, Value ms, Value _v1, Value _v2, Value _v3, Value _v4) { if(ms.type==VAL_INT) usleep(ms.data.i*1000); return val_nil(); }
Value nr_rt_args(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { Value a=val_arr(); for(int i=0;i<nr_argc;i++) nr_rt_push(val_nil(),a,val_str(nr_argv[i]),val_nil(),val_nil(),val_nil()); return a; }
Value nr_rt_exit(Value self, Value code, Value _v1, Value _v2, Value _v3, Value _v4) { exit(code.type==VAL_INT?(int)code.data.i:0); return val_nil(); }
Value nr_rt_to_float(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_FLOAT) return v; if(v.type==VAL_STR) return (Value){.type=VAL_FLOAT,.data.f=atof(v.data.s)}; if(v.type==VAL_INT) return (Value){.type=VAL_FLOAT,.data.f=(double)v.data.i}; return (Value){.type=VAL_FLOAT,.data.f=0}; }
Value nr_rt_to_bool(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { return val_bool(IS_TRUTHY(v)); }
Value nr_rt_parse_int(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_STR) return val_int(atoll(v.data.s)); return val_int(0); }
Value nr_rt_parse_float(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_STR) return (Value){.type=VAL_FLOAT,.data.f=atof(v.data.s)}; return (Value){.type=VAL_FLOAT,.data.f=0}; }
Value nr_rt_parse_bool(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_STR) { if(strcmp(v.data.s,"true")==0) return val_bool(true); if(strcmp(v.data.s,"false")==0) return val_bool(false); } return val_bool(false); }
const char* B64T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
Value nr_rt_to_base64(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type!=VAL_STR) return val_nil(); int l=strlen(v.data.s); char* res=nr_alloc(4*((l+2)/3)+1); int i=0,j=0; while(i<l){ uint32_t a=v.data.s[i++]; uint32_t b=i<l?v.data.s[i++]:0; uint32_t c=i<l?v.data.s[i++]:0; uint32_t t=(a<<16)|(b<<8)|c; res[j++]=B64T[(t>>18)&0x3F]; res[j++]=B64T[(t>>12)&0x3F]; res[j++]=i>l+1?'=':B64T[(t>>6)&0x3F]; res[j++]=i>l?'=':B64T[t&0x3F]; } res[j]=0; return val_str(res); }
Value nr_rt_from_base64(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type!=VAL_STR) return val_nil(); int l=strlen(v.data.s); if(l%4!=0) return val_nil(); int ol=l/4*3; if(v.data.s[l-1]=='=') ol--; if(v.data.s[l-2]=='=') ol--; char* res=nr_alloc(ol+1); static char dt[256]; static int built=0; if(!built){ for(int i=0;i<64;i++) dt[(unsigned char)B64T[i]]=i; built=1; } int i=0,j=0; while(i<l){ uint32_t a=dt[(unsigned char)v.data.s[i++]]; uint32_t b=dt[(unsigned char)v.data.s[i++]]; uint32_t c=v.data.s[i]=='='?0:dt[(unsigned char)v.data.s[i++]]; if(v.data.s[i-1]!='=') i--; i++; uint32_t d=v.data.s[i]=='='?0:dt[(unsigned char)v.data.s[i++]]; if(v.data.s[i-1]!='=') i--; i++; uint32_t t=(a<<18)|(b<<12)|(c<<6)|d; if(j<ol) res[j++]=(t>>16)&0xFF; if(j<ol) res[j++]=(t>>8)&0xFF; if(j<ol) res[j++]=t&0xFF; } res[ol]=0; return val_str(res); }
Value nr_rt_to_set(Value self, Value arr, Value _v1, Value _v2, Value _v3, Value _v4) { if(arr.type!=VAL_ARR) return val_arr(); Value res=val_arr(); for(int i=0;i<arr.data.arr->count;i++){ Value v=arr.data.arr->elements[i]; int f=0; for(int j=0;j<res.data.arr->count;j++) if(res.data.arr->elements[j].type==v.type && res.data.arr->elements[j].data.i==v.data.i) { f=1; break; } if(!f) nr_rt_push(val_nil(),res,v,val_nil(),val_nil(),val_nil()); } return res; }
Value nr_rt_keys(Value self, Value obj, Value _v1, Value _v2, Value _v3, Value _v4) { if(obj.type!=VAL_OBJ) return val_arr(); Value res=val_arr(); for(int i=0;i<obj.data.obj->count;i++) if(obj.data.obj->keys[i]) nr_rt_push(val_nil(), res, val_str(nr_strdup(obj.data.obj->keys[i])), val_nil(), val_nil(), val_nil()); return res; }
Value nr_rt_sqlite3_open(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_rt_sqlite3_close(Value self, Value db_v, Value _v1, Value _v2, Value _v3, Value _v4);
Value nr_rt_sqlite3_exec(Value self, Value db_v, Value sql, Value params, Value _v3, Value _v4);
Value nr_rt_sqlite3_query(Value self, Value db_v, Value sql, Value params, Value _v3, Value _v4);
Value nr_rt_sqlite3_open(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4) {
  if(path.type!=VAL_STR) return val_nil(); sqlite3* db; if(sqlite3_open(path.data.s, &db)!=SQLITE_OK) return val_nil();
  Value conn = val_int((long long)db); Value m = val_obj(); set_field(m, "_conn", conn);
  set_field(m, "exec", val_func(nr_rt_sqlite3_exec)); set_field(m, "query", val_func(nr_rt_sqlite3_query)); set_field(m, "close", val_func(nr_rt_sqlite3_close)); return m;
}
Value nr_rt_sqlite3_close(Value self, Value db_v, Value _v1, Value _v2, Value _v3, Value _v4) { Value conn = (self.type==VAL_OBJ) ? get_field(self, "_conn") : db_v; if(conn.type!=VAL_INT) return val_nil(); sqlite3_close((sqlite3*)conn.data.i); return val_nil(); }
Value nr_rt_sqlite3_exec(Value self, Value db_v, Value sql_v, Value params_v, Value _v3, Value _v4) {
  Value conn = (self.type==VAL_OBJ) ? get_field(self, "_conn") : db_v;
  Value sql = (self.type==VAL_OBJ) ? db_v : sql_v;
  Value params = (self.type==VAL_OBJ) ? sql_v : params_v;
  if(conn.type!=VAL_INT || sql.type!=VAL_STR) return val_nil(); sqlite3* db=(sqlite3*)conn.data.i; sqlite3_stmt* stmt; if(sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL)!=SQLITE_OK) return val_nil();
  if(params.type==VAL_ARR) for(int i=0;i<params.data.arr->count;i++){ Value p=params.data.arr->elements[i]; if(p.type==VAL_INT) sqlite3_bind_int64(stmt, i+1, p.data.i); else if(p.type==VAL_FLOAT) sqlite3_bind_double(stmt, i+1, p.data.f); else if(p.type==VAL_STR) sqlite3_bind_text(stmt, i+1, p.data.s, -1, SQLITE_TRANSIENT); else if(p.type==VAL_NIL) sqlite3_bind_null(stmt, i+1); }
  int res=sqlite3_step(stmt); sqlite3_finalize(stmt); return val_bool(res==SQLITE_DONE || res==SQLITE_OK); }
Value nr_rt_sqlite3_query(Value self, Value db_v, Value sql_v, Value params_v, Value _v3, Value _v4) {
  Value conn = (self.type==VAL_OBJ) ? get_field(self, "_conn") : db_v;
  Value sql = (self.type==VAL_OBJ) ? db_v : sql_v;
  Value params = (self.type==VAL_OBJ) ? sql_v : params_v;
  if(conn.type!=VAL_INT || sql.type!=VAL_STR) return val_nil(); sqlite3* db=(sqlite3*)conn.data.i; sqlite3_stmt* stmt; if(sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, NULL)!=SQLITE_OK) return val_nil();
  if(params.type==VAL_ARR) for(int i=0;i<params.data.arr->count;i++){ Value p=params.data.arr->elements[i]; if(p.type==VAL_INT) sqlite3_bind_int64(stmt, i+1, p.data.i); else if(p.type==VAL_FLOAT) sqlite3_bind_double(stmt, i+1, p.data.f); else if(p.type==VAL_STR) sqlite3_bind_text(stmt, i+1, p.data.s, -1, SQLITE_TRANSIENT); else if(p.type==VAL_NIL) sqlite3_bind_null(stmt, i+1); }
  Value res_arr=val_arr(); int cols=sqlite3_column_count(stmt); while(sqlite3_step(stmt)==SQLITE_ROW){ Value row=val_obj(); for(int i=0;i<cols;i++){ const char* name=sqlite3_column_name(stmt, i); int type=sqlite3_column_type(stmt, i); Value val; if(type==SQLITE_INTEGER) val=val_int(sqlite3_column_int64(stmt, i)); else if(type==SQLITE_FLOAT) val=(Value){.type=VAL_FLOAT,.data.f=sqlite3_column_double(stmt, i)}; else if(type==SQLITE_TEXT) val=val_str(nr_strdup((const char*)sqlite3_column_text(stmt, i))); else val=val_nil(); set_field(row, name, val); } nr_rt_push(val_nil(), res_arr, row, val_nil(), val_nil(), val_nil()); } sqlite3_finalize(stmt); return res_arr; }

Value nr_rt_http_serve(Value self, Value port_v, Value routes_v, Value _v3, Value _v4, Value _v5) {
  int server_fd; struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(port_v.data.i);
  while(1) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0); if(server_fd<0){perror("socket");return val_nil();}
    int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    if(bind(server_fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
      if(errno==EADDRINUSE) { printf("⚠️  Port %lld is busy, retrying in 2s...\n", port_v.data.i); fflush(stdout); close(server_fd); sleep(2); continue; }
      perror("bind"); close(server_fd); return val_nil();
    } break;
  }
  if(listen(server_fd,3)<0){perror("listen");return val_nil();}
  printf("Nira Server listening on port %lld...\n", port_v.data.i); fflush(stdout);
  while(1) {
    int ns=accept(server_fd,NULL,NULL); if(ns<0) continue;
    char buf[16384]={0}; int total_read = 0;
    while(total_read < 16383) {
        int n = read(ns, buf + total_read, 16383 - total_read);
        if(n <= 0) break;
        total_read += n;
        if(strstr(buf, "\r\n\r\n")) {
            char* cl_p = strstr(buf, "Content-Length: ");
            if(cl_p) {
                int cl = atoi(cl_p + 16);
                char* body_p = strstr(buf, "\r\n\r\n") + 4;
                if(total_read - (body_p - buf) >= cl) break;
            } else break;
        }
    }
    char m[16]={0},p[512]={0}; sscanf(buf,"%15s %511s",m,p);
    Value req=val_obj(); set_field(req,"method",val_str(nr_strdup(m))); set_field(req,"path",val_str(nr_strdup(p)));
    char* bp=strstr(buf,"\r\n\r\n"); set_field(req,"body",val_str(bp?nr_strdup(bp+4):""));
    Value res_v=val_nil(); int matched=0;
    if(routes_v.type==VAL_ARR) {
      for(int ri=0;ri<routes_v.data.arr->count;ri++) {
        Value route=routes_v.data.arr->elements[ri]; if(route.type!=VAL_OBJ) continue;
        Value rm=get_field(route,"method"),rp=get_field(route,"path"),rh=get_field(route,"handler");
        if(rm.type!=VAL_STR||rp.type!=VAL_STR||strcmp(rm.data.s,m)!=0) continue;
        int pm=0; if(strcmp(rp.data.s,p)==0) pm=1;
        else { char* c=strchr(rp.data.s,':'); if(c) { int pl=(int)(c-rp.data.s); if(strncmp(rp.data.s,p,pl)==0) { Value pa=val_obj(); set_field(pa,c+1,val_str(nr_strdup(p+pl))); set_field(req,"params",pa); pm=1; } } }
        if(!pm) continue; matched=1;
        if(rh.type==VAL_FUNC) res_v=((Value(*)(Value,Value,Value,Value,Value,Value))rh.data.func_ptr)(val_nil(),req,val_nil(),val_nil(),val_nil(),val_nil());
        break;
      }
    }
    char* body="Not Found"; char* ct="text/html"; int status=matched?200:404;
    if(res_v.type==VAL_OBJ) { Value b=get_field(res_v,"body"); if(b.type==VAL_STR) body=b.data.s; Value st=get_field(res_v,"status"); if(st.type==VAL_INT) status=st.data.i; Value ctt=get_field(res_v,"content_type"); if(ctt.type==VAL_STR) ct=ctt.data.s; }
    else if(res_v.type==VAL_STR) { body=res_v.data.s; status=200; }
    int bl=strlen(body); char hdr[512]; int hl=snprintf(hdr,sizeof(hdr),"HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",status,ct,bl);
    write(ns,hdr,hl); write(ns,body,bl); close(ns);
  } return val_nil();
}
Value nr_rt_http_add_get(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {
    Value routes = get_field(self, "_routes");
    Value route = val_obj();
    set_field(route, "method", val_str("GET"));
    set_field(route, "path", path);
    set_field(route, "handler", handler);
    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());
    return val_nil();
}
Value nr_rt_http_add_post(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {
    Value routes = get_field(self, "_routes");
    Value route = val_obj();
    set_field(route, "method", val_str("POST"));
    set_field(route, "path", path);
    set_field(route, "handler", handler);
    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());
    return val_nil();
}
Value nr_rt_http_add_delete(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {
    Value routes = get_field(self, "_routes");
    Value route = val_obj();
    set_field(route, "method", val_str("DELETE"));
    set_field(route, "path", path);
    set_field(route, "handler", handler);
    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());
    return val_nil();
}
Value nr_rt_http_listen(Value self, Value port, Value _v1, Value _v2, Value _v3, Value _v4) {
    Value routes = get_field(self, "_routes");
    return nr_rt_http_serve(val_nil(), port, routes, val_nil(), val_nil(), val_nil());
}
Value nr_rt_http_app(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {
    Value app = val_obj();
    set_field(app, "_routes", val_arr());
    set_field(app, "get", val_func(nr_rt_http_add_get));
    set_field(app, "post", val_func(nr_rt_http_add_post));
    set_field(app, "delete", val_func(nr_rt_http_add_delete));
    set_field(app, "listen", val_func(nr_rt_http_listen));
    return app;
}
Value nr_rt_load_module(const char* name) {
  if (strcmp(name, "time") == 0) { Value m = val_obj(); set_field(m, "now", val_func(nr_rt_now)); set_field(m, "millis", val_func(nr_rt_millis)); set_field(m, "toUnix", val_func(nr_rt_time_to_unix)); set_field(m, "fromDate", val_func(nr_rt_from_date)); return m; }
  if (strcmp(name, "math") == 0) { Value m = val_obj(); set_field(m, "sqrt", val_func(nr_rt_sqrt)); set_field(m, "random", val_func(nr_rt_random)); return m; }
  if (strcmp(name, "json") == 0) { Value m = val_obj(); set_field(m, "parse", val_func(nr_rt_json_parse)); set_field(m, "stringify", val_func(nr_rt_json_encode)); set_field(m, "encode", val_func(nr_rt_json_encode)); return m; }
  if (strcmp(name, "file") == 0) { Value m = val_obj(); set_field(m, "read", val_func(nr_rt_file_read)); set_field(m, "write", val_func(nr_rt_file_write)); set_field(m, "remove", val_func(nr_rt_file_delete)); set_field(m, "exists", val_func(nr_rt_file_exists)); set_field(m, "append", val_func(nr_rt_file_append)); return m; }
  if (strcmp(name, "sys") == 0) { Value m = val_obj(); set_field(m, "run", val_func(nr_rt_sys_run)); set_field(m, "args", val_func(nr_rt_args)); set_field(m, "exit", val_func(nr_rt_exit)); return m; }
  if (strcmp(name, "http") == 0) { Value m = val_obj(); set_field(m, "app", val_func(nr_rt_http_app)); return m; }
  if (strcmp(name, "conv") == 0) { Value m = val_obj(); set_field(m, "toInt", val_func(nr_rt_to_int)); set_field(m, "toFloat", val_func(nr_rt_to_float)); set_field(m, "toStr", val_func(nr_rt_to_string)); set_field(m, "toBool", val_func(nr_rt_to_bool)); return m; }
  if (strcmp(name, "parse") == 0) { Value m = val_obj(); set_field(m, "parseInt", val_func(nr_rt_parse_int)); set_field(m, "parseFloat", val_func(nr_rt_parse_float)); set_field(m, "parseBool", val_func(nr_rt_parse_bool)); return m; }
  if (strcmp(name, "encoding") == 0) { Value m = val_obj(); set_field(m, "toBase64", val_func(nr_rt_to_base64)); set_field(m, "fromBase64", val_func(nr_rt_from_base64)); return m; }
  if (strcmp(name, "collection") == 0) { Value m = val_obj(); set_field(m, "toSet", val_func(nr_rt_to_set)); return m; }
  if (strcmp(name, "sqlite3") == 0) { Value m = val_obj(); set_field(m, "open", val_func(nr_rt_sqlite3_open)); set_field(m, "close", val_func(nr_rt_sqlite3_close)); set_field(m, "exec", val_func(nr_rt_sqlite3_exec)); set_field(m, "query", val_func(nr_rt_sqlite3_query)); return m; }
  return val_obj();
}
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
Value nr_v_toInt;
Value nr_v_json;
Value nr_v_file;
Value nr_v_http;
Value nr_v_items;
Value nr_v_min_val;
Value nr_v_x;
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
  nr_v_toInt = val_nil();
  nr_v_json = val_nil();
  nr_v_file = val_nil();
  nr_v_http = val_nil();
  nr_v_items = val_nil();
  nr_v_min_val = val_nil();
  nr_v_x = val_nil();
  nr_v_toInt = val_func(nr_rt_to_int);
  nr_v_json = nr_rt_load_module("json");
  nr_v_file = nr_rt_load_module("file");
  nr_v_http = nr_rt_load_module("http");
val_func(nr_main);
  nr_v_items = ({ Value _a = val_arr(); nr_rt_push(val_nil(), _a, val_int(5), val_nil(), val_nil(), val_nil()); nr_rt_push(val_nil(), _a, val_int(2), val_nil(), val_nil(), val_nil()); nr_rt_push(val_nil(), _a, val_int(9), val_nil(), val_nil(), val_nil()); nr_rt_push(val_nil(), _a, val_int(1), val_nil(), val_nil(), val_nil()); nr_rt_push(val_nil(), _a, val_int(5), val_nil(), val_nil(), val_nil()); nr_rt_push(val_nil(), _a, val_int(6), val_nil(), val_nil(), val_nil()); _a; });
  nr_v_n = (nr_rt_len(val_nil(), nr_v_items, val_nil(), val_nil(), val_nil(), val_nil())).data.i;
  nr_v_min_val = nr_rt_at(val_nil(), nr_v_items, val_int(0), val_nil(), val_nil(), val_nil());
  { Value _iter = nr_v_items; if (_iter.type == VAL_ARR) {
    for (int _i = 0; _i < _iter.data.arr->count; _i++) {
      nr_v_x = _iter.data.arr->elements[_i];
  if (((nr_v_x).data.i < (nr_v_min_val).data.i)) {
  nr_v_min_val = nr_v_x;
  };
    }
  } }
;
({ nr_rt_print(val_nil(), nr_v_min_val, val_nil(), val_nil(), val_nil(), val_nil()); val_nil(); });
  return 0; 
}
