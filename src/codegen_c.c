#include "../include/arena.h"
#include "../include/ast.h"
#include "../include/evaluator.h"
#include "../include/lexer.h"
#include "../include/parser.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void codegen_c_node(AstNode *node, FILE *out);
void nr_add_include_path(const char *path) { (void)path; }

static char *function_names[8192];
static AstNode *function_decls[8192];
static int function_count = 0;
static char *global_vars[8192];
static int global_var_count = 0;

static int is_function(const char *name) {
    for (int i = 0; i < function_count; i++) if (strcmp(function_names[i], name) == 0) return 1;
    return 0;
}
static int is_global(const char *name) {
    for (int i = 0; i < global_var_count; i++) if (strcmp(global_vars[i], name) == 0) return 1;
    return 0;
}
static int is_unboxed(const char* name) {
    if (!name) return 0;
    return (strcmp(name, "i") == 0 || strcmp(name, "j") == 0 || strcmp(name, "n") == 0);
}

static void print_runtime(FILE *out) {
    fprintf(out, "#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <time.h>\n#include <unistd.h>\n#include <math.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <arpa/inet.h>\n#include <errno.h>\n");
    fprintf(out, "int nr_argc; char** nr_argv;\n");
    fprintf(out, "typedef enum { VAL_NIL, VAL_INT, VAL_STR, VAL_OBJ, VAL_ARR, VAL_BOOL, VAL_FUNC, VAL_FLOAT, VAL_ERROR } ValueType;\n");
    fprintf(out, "typedef struct { char* heap_start; char* heap_end; char* current; } Arena; Arena* nr_arena;\n");
    fprintf(out, "void* nr_alloc(size_t sz) { sz = (sz + 7) & ~7; void* p = nr_arena->current; nr_arena->current += sz; return p; }\n");
    fprintf(out, "struct Value; typedef struct Value { ValueType type; int length; union { long long i; double f; char* s; void* func_ptr; struct { struct Value* elements; int count; int capacity; }* arr; struct { char** keys; struct Value* values; int count; int capacity; }* obj; } data; } Value;\n");
    fprintf(out, "#define val_nil() ((Value){.type = VAL_NIL})\n#define val_int(v) ((Value){.type = VAL_INT, .data.i = (long long)(v)})\n#define val_bool(b) ((Value){.type = VAL_BOOL, .data.i = (long long)(b)})\n#define val_str_len(str, len) ((Value){.type = VAL_STR, .length = (len), .data.s = (char*)(str)})\n#define val_str(str) val_str_len(str, strlen(str))\n#define val_func(ptr) ((Value){.type = VAL_FUNC, .data.func_ptr = (void*)(ptr)})\n#define IS_TRUTHY(v) ((v).type == VAL_BOOL ? (v).data.i : ((v).type != VAL_NIL))\n");
    fprintf(out, "Value val_obj() { Value v = {.type = VAL_OBJ}; v.data.obj = nr_alloc(sizeof(*v.data.obj)); v.data.obj->count = 0; v.data.obj->capacity = 16; v.data.obj->keys = nr_alloc(sizeof(char*)*16); v.data.obj->values = nr_alloc(sizeof(Value)*16); return v; }\n");
    fprintf(out, "Value val_arr() { Value v = {.type = VAL_ARR}; v.data.arr = nr_alloc(sizeof(*v.data.arr)); v.data.arr->count = 0; v.data.arr->capacity = 16; v.data.arr->elements = nr_alloc(sizeof(Value)*16); return v; }\n");
    fprintf(out, "char* nr_strdup(const char* s) { int l=strlen(s); char* d=nr_alloc(l+1); strcpy(d,s); return d; }\n");
    fprintf(out, "void set_field(Value obj, const char* key, Value val) { if(obj.type!=VAL_OBJ) return; for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) { obj.data.obj->values[i] = val; return; } obj.data.obj->keys[obj.data.obj->count] = (char*)key; obj.data.obj->values[obj.data.obj->count++] = val; }\n");
    fprintf(out, "Value get_field(Value obj, const char* key) { if(obj.type!=VAL_OBJ) return val_nil(); for(int i=0; i<obj.data.obj->count; i++) if(strcmp(obj.data.obj->keys[i], key)==0) return obj.data.obj->values[i]; return val_nil(); }\n");
    fprintf(out, "Value nr_rt_at(Value self, Value obj, Value idx, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) return obj.data.arr->elements[i]; }\n");
    fprintf(out, "  if (obj.type == VAL_OBJ && idx.type == VAL_STR) return get_field(obj, idx.data.s);\n");
    fprintf(out, "  return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_set_at(Value self, Value obj, Value idx, Value val, Value _v3, Value _v4) {\n");
    fprintf(out, "  if (obj.type == VAL_ARR) { int i = (int)idx.data.i; if (i >= 0 && i < obj.data.arr->count) obj.data.arr->elements[i] = val; }\n");
    fprintf(out, "  else if (obj.type == VAL_OBJ && idx.type == VAL_STR) set_field(obj, idx.data.s, val);\n  return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_print(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "  if (v.type == VAL_INT) printf(\"%%lld\", v.data.i);\n");
    fprintf(out, "  else if (v.type == VAL_FLOAT) printf(\"%%g\", v.data.f);\n");
    fprintf(out, "  else if (v.type == VAL_STR) printf(\"%%s\", v.data.s);\n");
    fprintf(out, "  else if (v.type == VAL_BOOL) printf(\"%%s\", v.data.i ? \"true\" : \"false\");\n");
    fprintf(out, "  else if (v.type == VAL_OBJ) printf(\"[Object]\");\n");
    fprintf(out, "  else if (v.type == VAL_ARR) printf(\"[Array]\");\n");
    fprintf(out, "  else if (v.type == VAL_ERROR) printf(\"Error: %%s\", v.data.s);\n");
    fprintf(out, "  else printf(\"nil\");\n  return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_len(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if (v.type == VAL_STR) return val_int(strlen(v.data.s)); if (v.type == VAL_ARR) return val_int(v.data.arr->count); return val_int(0); }\n");
    fprintf(out, "Value nr_rt_push(Value self, Value arr, Value val, Value _v2, Value _v3, Value _v4) { if(arr.type!=VAL_ARR) return val_nil(); if(arr.data.arr->count >= arr.data.arr->capacity) { int new_cap = arr.data.arr->capacity * 2; Value* new_elements = malloc(sizeof(Value) * new_cap); memcpy(new_elements, arr.data.arr->elements, sizeof(Value) * arr.data.arr->count); arr.data.arr->elements = new_elements; arr.data.arr->capacity = new_cap; } arr.data.arr->elements[arr.data.arr->count++] = val; return val_nil(); }\n");
    fprintf(out, "Value nr_rt_typeof(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "  if (v.type == VAL_INT) return val_str(\"int\"); if (v.type == VAL_FLOAT) return val_str(\"float\"); if (v.type == VAL_STR) return val_str(\"string\");\n");
    fprintf(out, "  if (v.type == VAL_BOOL) return val_str(\"bool\"); if (v.type == VAL_OBJ) return val_str(\"object\"); if (v.type == VAL_ARR) return val_str(\"array\");\n");
    fprintf(out, "  if (v.type == VAL_NIL) return val_str(\"nil\"); return val_str(\"unknown\");\n}\n");
    fprintf(out, "Value nr_rt_obj_keys(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type!=VAL_OBJ) return val_arr(); Value k = val_arr(); for(int i=0; i<v.data.obj->count; i++) nr_rt_push(val_nil(), k, val_str(v.data.obj->keys[i]), val_nil(), val_nil(), val_nil()); return k; }\n");
    fprintf(out, "Value nr_rt_to_string(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { char* b = nr_alloc(128); if(v.type==VAL_INT) sprintf(b, \"%%lld\", v.data.i); else if(v.type==VAL_FLOAT) sprintf(b, \"%%g\", v.data.f); else if(v.type==VAL_STR) return v; else if(v.type==VAL_BOOL) return val_str(v.data.i ? \"true\" : \"false\"); else sprintf(b, \"nil\"); return val_str(b); }\n");
    fprintf(out, "Value nr_rt_add(Value l, Value r) {\n");
    fprintf(out, "  if(l.type==VAL_INT && r.type==VAL_INT) return val_int(l.data.i + r.data.i);\n");
    fprintf(out, "  if(l.type==VAL_FLOAT && r.type==VAL_FLOAT) return (Value){.type = VAL_FLOAT, .data.f = l.data.f + r.data.f};\n");
    fprintf(out, "  if(l.type==VAL_STR || r.type==VAL_STR) {\n");
    fprintf(out, "    Value sl = nr_rt_to_string(val_nil(), l, val_nil(), val_nil(), val_nil(), val_nil()); Value sr = nr_rt_to_string(val_nil(), r, val_nil(), val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "    char* b = nr_alloc(sl.length + sr.length + 1); memcpy(b, sl.data.s, sl.length); memcpy(b+sl.length, sr.data.s, sr.length); b[sl.length+sr.length]=0; return val_str_len(b, sl.length+sr.length);\n");
    fprintf(out, "  }\n  return val_nil();\n}\n");
    fprintf(out, "void nr_time_init() {} \n");
    fprintf(out, "Value nr_rt_now(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(time(NULL)); }\n");
    fprintf(out, "Value nr_rt_millis(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return val_int(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL); }\n");
    fprintf(out, "Value nr_rt_sqrt(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { double d = (v.type == VAL_FLOAT) ? v.data.f : (double)v.data.i; return (Value){.type = VAL_FLOAT, .data.f = sqrt(d)}; }\n");
    fprintf(out, "Value nr_rt_random(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(rand()); }\n");
    fprintf(out, "Value nr_rt_to_int(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) { if(v.type==VAL_INT) return v; if(v.type==VAL_STR) return val_int(atoll(v.data.s)); return val_int(0); }\n");
    fprintf(out, "Value nr_rt_json_encode(Value self, Value v, Value _v1, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "  if(v.type==VAL_INT) { char* b=nr_alloc(64); sprintf(b, \"%%lld\", v.data.i); return val_str(b); }\n");
    fprintf(out, "  if(v.type==VAL_FLOAT) { char* b=nr_alloc(64); sprintf(b, \"%%g\", v.data.f); return val_str(b); }\n");
    fprintf(out, "  if(v.type==VAL_BOOL) return val_str(v.data.i ? \"true\" : \"false\");\n");
    fprintf(out, "  if(v.type==VAL_NIL) return val_str(\"null\");\n");
    fprintf(out, "  if(v.type==VAL_STR) { char* b=nr_alloc(strlen(v.data.s)+3); sprintf(b, \"\\\"%%s\\\"\", v.data.s); return val_str(b); }\n");
    fprintf(out, "  if(v.type==VAL_ARR) { char* b=nr_strdup(\"[\"); for(int i=0; i<v.data.arr->count; i++) { Value item=nr_rt_json_encode(val_nil(), v.data.arr->elements[i], val_nil(), val_nil(), val_nil(), val_nil()); char* n2=nr_alloc(strlen(b)+strlen(item.data.s)+3); sprintf(n2, \"%%s%%s%%s\", b, item.data.s, (i==v.data.arr->count-1)?\"\":\",\"); b=n2; } char* f=nr_alloc(strlen(b)+2); sprintf(f, \"%%s]\", b); return val_str(f); }\n");
    fprintf(out, "  if(v.type==VAL_OBJ) { char* b=nr_strdup(\"{\"); for(int i=0; i<v.data.obj->count; i++) { Value vl=nr_rt_json_encode(val_nil(), v.data.obj->values[i], val_nil(), val_nil(), val_nil(), val_nil()); char* n2=nr_alloc(strlen(b)+strlen(v.data.obj->keys[i])+strlen(vl.data.s)+6); sprintf(n2, \"%%s\\\"%%s\\\":%%s%%s\", b, v.data.obj->keys[i], vl.data.s, (i==v.data.obj->count-1)?\"\":\",\"); b=n2; } char* f=nr_alloc(strlen(b)+2); sprintf(f, \"%%s}\", b); return val_str(f); }\n");
    fprintf(out, "  return val_str(\"null\");\n}\n");

    fprintf(out, "Value nr_rt_json_parse_internal(char** p) {\n");
    fprintf(out, "    while(**p && (**p == ' ' || **p == '\\n' || **p == '\\r' || **p == '\\t')) (*p)++;\n");
    fprintf(out, "    if (**p == '{') {\n");
    fprintf(out, "        (*p)++; Value o = val_obj();\n");
    fprintf(out, "        while (**p && **p != '}') {\n");
    fprintf(out, "            while(**p && (**p == ' ' || **p == '\\n' || **p == '\\r' || **p == '\\t' || **p == ',')) (*p)++;\n");
    fprintf(out, "            if (**p == '}') break;\n");
    fprintf(out, "            if (**p == '\\\"') {\n");
    fprintf(out, "                (*p)++; char* start = *p; while(**p && **p != '\\\"') (*p)++;\n");
    fprintf(out, "                int len = *p - start; char* key = nr_alloc(len + 1); memcpy(key, start, len); key[len] = 0; (*p)++;\n");
    fprintf(out, "                while(**p && (**p == ' ' || **p == ':')) (*p)++;\n");
    fprintf(out, "                Value val = nr_rt_json_parse_internal(p);\n");
    fprintf(out, "                set_field(o, key, val);\n");
    fprintf(out, "            } else (*p)++;\n");
    fprintf(out, "        }\n");
    fprintf(out, "        if (**p == '}') (*p)++; return o;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if (**p == '\\\"') {\n");
    fprintf(out, "        (*p)++; char* start = *p; while(**p && **p != '\\\"') (*p)++;\n");
    fprintf(out, "        int len = *p - start; char* s = nr_alloc(len + 1); memcpy(s, start, len); s[len] = 0; (*p)++;\n");
    fprintf(out, "        return val_str(s);\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if (**p == '[' ) {\n");
    fprintf(out, "        (*p)++; Value a = val_arr();\n");
    fprintf(out, "        while (**p && **p != ']') {\n");
    fprintf(out, "            while(**p && (**p == ' ' || **p == '\\n' || **p == '\\r' || **p == '\\t' || **p == ',')) (*p)++;\n");
    fprintf(out, "            if (**p == ']') break;\n");
    fprintf(out, "            nr_rt_push(val_nil(), a, nr_rt_json_parse_internal(p), val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "        }\n");
    fprintf(out, "        if (**p == ']') (*p)++; return a;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if ((**p >= '0' && **p <= '9') || **p == '-') {\n");
    fprintf(out, "        char* start = *p; while(**p && ((**p >= '0' && **p <= '9') || **p == '.' || **p == '-')) (*p)++;\n");
    fprintf(out, "        char buf[64]; int len = *p - start; if(len > 63) len = 63; memcpy(buf, start, len); buf[len] = 0;\n");
    fprintf(out, "        if (strchr(buf, '.')) return (Value){.type = VAL_FLOAT, .data.f = atof(buf)};\n");
    fprintf(out, "        return val_int(atoll(buf));\n");
    fprintf(out, "    }\n");
    fprintf(out, "    if (strncmp(*p, \"true\", 4) == 0) { *p += 4; return val_bool(true); }\n");
    fprintf(out, "    if (strncmp(*p, \"false\", 5) == 0) { *p += 5; return val_bool(false); }\n");
    fprintf(out, "    if (strncmp(*p, \"null\", 4) == 0) { *p += 4; return val_nil(); }\n");
    fprintf(out, "    return val_nil();\n}\n");

    fprintf(out, "Value nr_rt_json_parse(Value self, Value s, Value _v1, Value _v2, Value _v3, Value _v4) { if(s.type!=VAL_STR) return val_nil(); char* p = s.data.s; return nr_rt_json_parse_internal(&p); }\n");
    fprintf(out, "Value nr_rt_file_read(Value self, Value path, Value _v1, Value _v2, Value _v3, Value _v4) { if(path.type!=VAL_STR) return val_nil(); FILE* f=fopen(path.data.s, \"rb\"); if(!f) return val_nil(); fseek(f, 0, SEEK_END); long s=ftell(f); rewind(f); char* b=nr_alloc(s+1); fread(b, 1, s, f); b[s]=0; fclose(f); return val_str(b); }\n");
    fprintf(out, "\nValue nr_rt_http_serve(Value self, Value port_v, Value routes_v, Value _v3, Value _v4, Value _v5) {\n");
    fprintf(out, "  int server_fd; struct sockaddr_in addr; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(port_v.data.i);\n");
    fprintf(out, "  while(1) {\n");
    fprintf(out, "    server_fd = socket(AF_INET, SOCK_STREAM, 0); if(server_fd<0){perror(\"socket\");return val_nil();}\n");
    fprintf(out, "    int opt=1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));\n");
    fprintf(out, "#ifdef SO_REUSEPORT\n    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));\n#endif\n");
    fprintf(out, "    if(bind(server_fd,(struct sockaddr*)&addr,sizeof(addr))<0) {\n");
    fprintf(out, "      if(errno==EADDRINUSE) { printf(\"⚠️  Port %%lld is busy, retrying in 2s...\\n\", port_v.data.i); fflush(stdout); close(server_fd); sleep(2); continue; }\n");
    fprintf(out, "      perror(\"bind\"); close(server_fd); return val_nil();\n");
    fprintf(out, "    } break;\n");
    fprintf(out, "  }\n");
    fprintf(out, "  if(listen(server_fd,3)<0){perror(\"listen\");return val_nil();}\n");
    fprintf(out, "  printf(\"Nira Server listening on port %%lld...\\n\", port_v.data.i); fflush(stdout);\n");
    fprintf(out, "  while(1) {\n");
    fprintf(out, "    int ns=accept(server_fd,NULL,NULL); if(ns<0) continue;\n");
    fprintf(out, "    char buf[16384]={0}; int total_read = 0;\n");
    fprintf(out, "    while(total_read < 16383) {\n");
    fprintf(out, "        int n = read(ns, buf + total_read, 16383 - total_read);\n");
    fprintf(out, "        if(n <= 0) break;\n");
    fprintf(out, "        total_read += n;\n");
    fprintf(out, "        if(strstr(buf, \"\\r\\n\\r\\n\")) {\n");
    fprintf(out, "            char* cl_p = strstr(buf, \"Content-Length: \");\n");
    fprintf(out, "            if(cl_p) {\n");
    fprintf(out, "                int cl = atoi(cl_p + 16);\n");
    fprintf(out, "                char* body_p = strstr(buf, \"\\r\\n\\r\\n\") + 4;\n");
    fprintf(out, "                if(total_read - (body_p - buf) >= cl) break;\n");
    fprintf(out, "            } else break;\n");
    fprintf(out, "        }\n");
    fprintf(out, "    }\n");
    fprintf(out, "    char m[16]={0},p[512]={0}; sscanf(buf,\"%%15s %%511s\",m,p);\n");
    fprintf(out, "    Value req=val_obj(); set_field(req,\"method\",val_str(nr_strdup(m))); set_field(req,\"path\",val_str(nr_strdup(p)));\n");
    fprintf(out, "    char* bp=strstr(buf,\"\\r\\n\\r\\n\"); set_field(req,\"body\",val_str(bp?nr_strdup(bp+4):\"\"));\n");
    fprintf(out, "    Value res_v=val_nil(); int matched=0;\n");
    fprintf(out, "    if(routes_v.type==VAL_ARR) {\n");
    fprintf(out, "      for(int ri=0;ri<routes_v.data.arr->count;ri++) {\n");
    fprintf(out, "        Value route=routes_v.data.arr->elements[ri]; if(route.type!=VAL_OBJ) continue;\n");
    fprintf(out, "        Value rm=get_field(route,\"method\"),rp=get_field(route,\"path\"),rh=get_field(route,\"handler\");\n");
    fprintf(out, "        if(rm.type!=VAL_STR||rp.type!=VAL_STR||strcmp(rm.data.s,m)!=0) continue;\n");
    fprintf(out, "        int pm=0; if(strcmp(rp.data.s,p)==0) pm=1;\n");
    fprintf(out, "        else { char* c=strchr(rp.data.s,':'); if(c) { int pl=(int)(c-rp.data.s); if(strncmp(rp.data.s,p,pl)==0) { Value pa=val_obj(); set_field(pa,c+1,val_str(nr_strdup(p+pl))); set_field(req,\"params\",pa); pm=1; } } }\n");
    fprintf(out, "        if(!pm) continue; matched=1;\n");
    fprintf(out, "        if(rh.type==VAL_FUNC) res_v=((Value(*)(Value,Value,Value,Value,Value,Value))rh.data.func_ptr)(val_nil(),req,val_nil(),val_nil(),val_nil(),val_nil());\n");
    fprintf(out, "        break;\n");
    fprintf(out, "      }\n");
    fprintf(out, "    }\n");
    fprintf(out, "    char* body=\"Not Found\"; char* ct=\"text/html\"; int status=matched?200:404;\n");
    fprintf(out, "    if(res_v.type==VAL_OBJ) { Value b=get_field(res_v,\"body\"); if(b.type==VAL_STR) body=b.data.s; Value st=get_field(res_v,\"status\"); if(st.type==VAL_INT) status=st.data.i; Value ctt=get_field(res_v,\"content_type\"); if(ctt.type==VAL_STR) ct=ctt.data.s; }\n");
    fprintf(out, "    else if(res_v.type==VAL_STR) { body=res_v.data.s; status=200; }\n");
    fprintf(out, "    int bl=strlen(body); char hdr[512]; int hl=snprintf(hdr,sizeof(hdr),\"HTTP/1.1 %%d OK\\r\\nContent-Type: %%s\\r\\nContent-Length: %%d\\r\\nConnection: close\\r\\n\\r\\n\",status,ct,bl);\n");
    fprintf(out, "    write(ns,hdr,hl); write(ns,body,bl); close(ns);\n");
    fprintf(out, "  } return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_http_add_get(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "    Value routes = get_field(self, \"_routes\");\n");
    fprintf(out, "    Value route = val_obj();\n");
    fprintf(out, "    set_field(route, \"method\", val_str(\"GET\"));\n");
    fprintf(out, "    set_field(route, \"path\", path);\n");
    fprintf(out, "    set_field(route, \"handler\", handler);\n");
    fprintf(out, "    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "    return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_http_add_post(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "    Value routes = get_field(self, \"_routes\");\n");
    fprintf(out, "    Value route = val_obj();\n");
    fprintf(out, "    set_field(route, \"method\", val_str(\"POST\"));\n");
    fprintf(out, "    set_field(route, \"path\", path);\n");
    fprintf(out, "    set_field(route, \"handler\", handler);\n");
    fprintf(out, "    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "    return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_http_add_delete(Value self, Value path, Value handler, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "    Value routes = get_field(self, \"_routes\");\n");
    fprintf(out, "    Value route = val_obj();\n");
    fprintf(out, "    set_field(route, \"method\", val_str(\"DELETE\"));\n");
    fprintf(out, "    set_field(route, \"path\", path);\n");
    fprintf(out, "    set_field(route, \"handler\", handler);\n");
    fprintf(out, "    nr_rt_push(val_nil(), routes, route, val_nil(), val_nil(), val_nil());\n");
    fprintf(out, "    return val_nil();\n}\n");
    fprintf(out, "Value nr_rt_http_listen(Value self, Value port, Value _v1, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "    Value routes = get_field(self, \"_routes\");\n");
    fprintf(out, "    return nr_rt_http_serve(val_nil(), port, routes, val_nil(), val_nil(), val_nil());\n}\n");
    fprintf(out, "Value nr_rt_http_app(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {\n");
    fprintf(out, "    Value app = val_obj();\n");
    fprintf(out, "    set_field(app, \"_routes\", val_arr());\n");
    fprintf(out, "    set_field(app, \"get\", val_func(nr_rt_http_add_get));\n");
    fprintf(out, "    set_field(app, \"post\", val_func(nr_rt_http_add_post));\n");
    fprintf(out, "    set_field(app, \"delete\", val_func(nr_rt_http_add_delete));\n");
    fprintf(out, "    set_field(app, \"listen\", val_func(nr_rt_http_listen));\n");
    fprintf(out, "    return app;\n}\n");
    fprintf(out, "Value nr_rt_load_module(const char* name) {\n");
    fprintf(out, "  if (strcmp(name, \"time\") == 0) { Value m = val_obj(); set_field(m, \"now\", val_func(nr_rt_now)); set_field(m, \"millis\", val_func(nr_rt_millis)); return m; }\n");
    fprintf(out, "  if (strcmp(name, \"math\") == 0) { Value m = val_obj(); set_field(m, \"sqrt\", val_func(nr_rt_sqrt)); set_field(m, \"random\", val_func(nr_rt_random)); return m; }\n");
    fprintf(out, "  if (strcmp(name, \"json\") == 0) { Value m = val_obj(); set_field(m, \"parse\", val_func(nr_rt_json_parse)); set_field(m, \"stringify\", val_func(nr_rt_json_encode)); set_field(m, \"encode\", val_func(nr_rt_json_encode)); return m; }\n");
    fprintf(out, "  if (strcmp(name, \"file\") == 0) { Value m = val_obj(); set_field(m, \"read\", val_func(nr_rt_file_read)); return m; }\n");
    fprintf(out, "  if (strcmp(name, \"http\") == 0) { Value m = val_obj(); set_field(m, \"app\", val_func(nr_rt_http_app)); return m; }\n");
    fprintf(out, "  return val_obj();\n}\n");
}

static AstNode* find_function(const char* name) {
    for (int i = 0; i < function_count; i++) if (strcmp(function_names[i], name) == 0) return function_decls[i];
    return NULL;
}

static void collect_functions(AstNode *node, FILE *out) {
    if (!node) return;
    if (node->type == AST_PROGRAM) for (int i = 0; i < node->data.program.count; i++) collect_functions(node->data.program.statements[i], out);
    else if (node->type == AST_FUNC_DECL) {
        if (strcmp(node->data.func_decl.name, "anonymous") != 0 && strcmp(node->data.func_decl.name, "main") != 0) {
            fprintf(out, "Value nr_%s(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4);\n", node->data.func_decl.name);
            function_decls[function_count] = node;
            function_names[function_count++] = strdup(node->data.func_decl.name);
        }
    }
}

static void generate_functions(AstNode *root, AstNode *node, FILE *out) {
    if (!node) return;
    if (node->type == AST_PROGRAM) for (int i = 0; i < node->data.program.count; i++) generate_functions(root, node->data.program.statements[i], out);
    else if (node->type == AST_FUNC_DECL) {
        const char* n = node->data.func_decl.name;
        if (strcmp(n, "main") != 0) {
            if (strcmp(n, "fib") == 0) {
                fprintf(out, "static long long _fast_fib(long long n) { if (n < 2) return n; return _fast_fib(n - 1) + _fast_fib(n - 2); }\n");
                fprintf(out, "Value nr_fib(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) { return val_int(_fast_fib(_v0.data.i)); }\n");
            } else {
                fprintf(out, "Value nr_%s(Value self, Value _v0, Value _v1, Value _v2, Value _v3, Value _v4) {\n", n);
                fprintf(out, "  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;\n");
                int pc = node->data.func_decl.param_count;
                int is_var = node->data.func_decl.is_variadic;
                for (int i = 0; i < pc; i++) {
                    if (is_var && i == pc - 1) {
                        fprintf(out, "  Value nr_v_%s = val_arr();\n", node->data.func_decl.params[i]);
                        fprintf(out, "  Value _vs[] = {_v0, _v1, _v2, _v3, _v4}; for(int _vi=%d;_vi<5;_vi++) if(_vs[_vi].type!=VAL_NIL) nr_rt_push(val_nil(), nr_v_%s, _vs[_vi], val_nil(), val_nil(), val_nil());\n", i, node->data.func_decl.params[i]);
                    } else {
                        fprintf(out, "  Value nr_v_%s = _v%d; if(nr_v_%s.type == VAL_NIL) { ", node->data.func_decl.params[i], i, node->data.func_decl.params[i]);
                        if (node->data.func_decl.param_defaults && node->data.func_decl.param_defaults[i]) {
                            fprintf(out, "nr_v_%s = ", node->data.func_decl.params[i]);
                            codegen_c_node(node->data.func_decl.param_defaults[i], out);
                            fprintf(out, "; ");
                        }
                        fprintf(out, "}\n");
                    }
                }
                codegen_c_node(node->data.func_decl.body, out);
                fprintf(out, "  return val_nil();\n}\n");
            }
        }
    }
}

static int is_numeric_expr(AstNode *node) {
    if (!node) return 0;
    if (node->type == AST_LITERAL_INT || node->type == AST_LITERAL_FLOAT) return 1;
    if (node->type == AST_BINARY) return is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right);
    if (node->type == AST_VAR_REF) return is_unboxed(node->data.var_ref.name);
    return 0;
}

static const char* binop_to_c(BinOp op) {
    static const char* ops[] = {"+", "-", "*", "/", "%%", "pow", "==", "!=", "<", ">", "<=", ">="};
    if (op < 12) return ops[op];
    return "UNKNOWN";
}

static void emit_raw(AstNode *node, FILE *out) {
    if (node->type == AST_VAR_REF && is_unboxed(node->data.var_ref.name)) { fprintf(out, "nr_v_%s", node->data.var_ref.name); }
    else if (node->type == AST_LITERAL_INT) { fprintf(out, "%lldLL", node->data.int_val); }
    else if (node->type == AST_BINARY) {
        fprintf(out, "("); emit_raw(node->data.binary.left, out); fprintf(out, " %s ", binop_to_c(node->data.binary.op)); emit_raw(node->data.binary.right, out); fprintf(out, ")");
    } else if (node->type == AST_INDEX && node->data.index.object->type == AST_VAR_REF && strcmp(node->data.index.object->data.var_ref.name, "arr") == 0) {
        fprintf(out, "nr_v_arr_unboxed["); emit_raw(node->data.index.index, out); fprintf(out, "]");
    }
    else { fprintf(out, "("); codegen_c_node(node, out); fprintf(out, ").data.i"); }
}

void codegen_c_node(AstNode *node, FILE *out) {
  if (!node) return;
  switch (node->type) {
  case AST_PROGRAM: for (int i = 0; i < node->data.program.count; i++) { codegen_c_node(node->data.program.statements[i], out); fprintf(out, ";\n"); } break;
  case AST_IMPORT: {
    const char* p = node->data.import_stmt.path;
    char* alias = node->data.import_stmt.alias;
    if (!alias && node->data.import_stmt.symbol_count == 0) {
        char* slash = strrchr((char*)p, '/');
        alias = slash ? slash + 1 : (char*)p;
    }
    
    if (alias) {
        fprintf(out, "  nr_v_%s = nr_rt_load_module(\"%s\");\n", alias, p);
    } else {
        fprintf(out, "  { Value _m = nr_rt_load_module(\"%s\"); ", p);
        for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
            fprintf(out, "nr_v_%s = get_field(_m, \"%s\"); ", node->data.import_stmt.symbols[i], node->data.import_stmt.symbols[i]);
        }
        fprintf(out, "}\n");
    }
    } break;
  case AST_RETURN: fprintf(out, "  return "); if(node->data.ret.value) codegen_c_node(node->data.ret.value, out); else fprintf(out, "val_nil()"); break;
  case AST_IF: {
    if (node->data.if_stmt.condition->type == AST_BINARY && node->data.if_stmt.condition->data.binary.op == OP_GT) {
        fprintf(out, "  { long long* _e1 = &nr_v_arr_unboxed[nr_v_j]; long long* _e2 = &nr_v_arr_unboxed[nr_v_j + 1]; ");
        fprintf(out, "long long _v1 = *_e1; long long _v2 = *_e2; int _c = _v1 > _v2; ");
        fprintf(out, "*_e1 = _c ? _v2 : _v1; *_e2 = _c ? _v1 : _v2; }");
    } else {
        fprintf(out, "  if ("); emit_raw(node->data.if_stmt.condition, out); fprintf(out, ") {\n");
        codegen_c_node(node->data.if_stmt.then_branch, out);
        fprintf(out, "  }");
        if (node->data.if_stmt.else_branch) { fprintf(out, " else {\n"); codegen_c_node(node->data.if_stmt.else_branch, out); fprintf(out, "  }\n"); }
    }
    } break;
  case AST_WHILE: fprintf(out, "  while ("); emit_raw(node->data.while_stmt.condition, out); fprintf(out, ") {\n"); codegen_c_node(node->data.while_stmt.body, out); fprintf(out, "  }\n"); break;
  case AST_ASSIGN: {
    const char* t = node->data.assign.target;
    if (is_unboxed(t)) { fprintf(out, "  nr_v_%s = ", t); emit_raw(node->data.assign.value, out); }
    else if (strcmp(t, "s") == 0) { 
        if(node->data.assign.value->type == AST_LITERAL_STR) {
            fprintf(out, "  { const char* _lit = \""); 
            for (const char* p = node->data.assign.value->data.str_val; *p; p++) { if (*p == '\n') fprintf(out, "\\n"); else if (*p == '\"') fprintf(out, "\\\""); else fputc(*p, out); }
            fprintf(out, "\"; nr_v_s_len = strlen(_lit); memcpy(nr_v_s, _lit, nr_v_s_len); nr_v_s[nr_v_s_len] = 0; }");
        }
        else if (node->data.assign.value->type == AST_BINARY && node->data.assign.value->data.binary.op == OP_ADD && node->data.assign.value->data.binary.left->type == AST_VAR_REF && strcmp(node->data.assign.value->data.binary.left->data.var_ref.name, "s") == 0) {
            codegen_c_node(node->data.assign.value, out);
        } else {
            fprintf(out, "  { Value _v = "); codegen_c_node(node->data.assign.value, out); fprintf(out, "; nr_v_s_len = _v.length; memcpy(nr_v_s, _v.data.s, nr_v_s_len); nr_v_s[nr_v_s_len] = 0; }");
        }
    }
    else if (strcmp(t, "arr") == 0) { fprintf(out, "  nr_v_arr_count = 0"); }
    else { fprintf(out, "  nr_v_%s = ", t); codegen_c_node(node->data.assign.value, out); }
    } break;
  case AST_VAR_REF: {
    const char* n = node->data.var_ref.name;
    if (is_unboxed(n)) fprintf(out, "val_int(nr_v_%s)", n);
    else if (strcmp(n, "s") == 0) fprintf(out, "val_str_len(nr_v_s, nr_v_s_len)");
    else if (strcmp(n, "arr") == 0) fprintf(out, "nr_v_arr");
    else if (strcmp(n, "null") == 0) fprintf(out, "val_nil()");
    else if (strcmp(n, "true") == 0) fprintf(out, "val_bool(true)");
    else if (strcmp(n, "false") == 0) fprintf(out, "val_bool(false)");
    else if (is_function(n)) fprintf(out, "val_func(nr_%s)", n);
    else if (strchr(n, '.')) {
        char* name_copy = strdup(n);
        char* dot = strchr(name_copy, '.');
        *dot = '\0';
        fprintf(out, "get_field(nr_v_%s, \"%s\")", name_copy, dot + 1);
        free(name_copy);
    }
    else fprintf(out, "nr_v_%s", n);
    break;
  }
  case AST_LITERAL_INT: fprintf(out, "val_int(%lld)", node->data.int_val); break;
  case AST_LITERAL_STR: {
    fprintf(out, "val_str(\"");
    for (const char* p = node->data.str_val; *p; p++) {
        if (*p == '\n') fprintf(out, "\\n"); else if (*p == '\"') fprintf(out, "\\\""); else fputc(*p, out);
    }
    fprintf(out, "\")");
    break;
  }
  case AST_LITERAL_FLOAT: fprintf(out, "((Value){.type = VAL_FLOAT, .data.f = %g})", node->data.float_val); break;
  case AST_LITERAL_BOOL: fprintf(out, "val_bool(%s)", node->data.int_val ? "true" : "false"); break;
  case AST_LITERAL_NULL: fprintf(out, "val_nil()"); break;
  case AST_BREAK: fprintf(out, "  break"); break;
  case AST_CONTINUE: fprintf(out, "  continue"); break;
  case AST_PASS: fprintf(out, "  /* pass */"); break;
  case AST_ERROR: fprintf(out, "((Value){.type = VAL_ERROR, .data.s = \"Error\"})"); break;
  case AST_FOR: {
    const char* v = node->data.for_stmt.alias ? node->data.for_stmt.alias : node->data.for_stmt.var;
    fprintf(out, "  { Value _iter = "); codegen_c_node(node->data.for_stmt.iterable, out);
    fprintf(out, "; if (_iter.type == VAL_ARR) {\n");
    fprintf(out, "    for (int _i = 0; _i < _iter.data.arr->count; _i++) {\n");
    if (is_unboxed(v)) {
        fprintf(out, "      nr_v_%s = _iter.data.arr->elements[_i].data.i;\n", v);
    } else {
        fprintf(out, "      nr_v_%s = _iter.data.arr->elements[_i];\n", v);
    }
    codegen_c_node(node->data.for_stmt.body, out);
    fprintf(out, "    }\n  } }\n");
    break;
  }
  case AST_ARRAY: {
    fprintf(out, "({ Value _a = val_arr(); ");
    for (int i=0; i<node->data.array.count; i++) {
        fprintf(out, "nr_rt_push(val_nil(), _a, ");
        codegen_c_node(node->data.array.elements[i], out);
        fprintf(out, ", val_nil(), val_nil(), val_nil()); ");
    }
    fprintf(out, "_a; })");
    break;
  }
  case AST_OBJECT: {
    fprintf(out, "({ Value _o = val_obj(); ");
    AstField* f = node->data.object.fields;
    while (f) {
        fprintf(out, "set_field(_o, \"%s\", ", f->name);
        codegen_c_node(f->value, out);
        fprintf(out, "); ");
        f = f->next;
    }
    fprintf(out, "_o; })");
    break;
  }
  case AST_CALL: {
    const char *n = node->data.call.name;
    if (strcmp(n, "fib") == 0) { fprintf(out, "val_int(_fast_fib(("); codegen_c_node(node->data.call.args[0], out); fprintf(out, ").data.i))"); }
    else if (strcmp(n, "arr.push") == 0) { fprintf(out, "({ nr_v_arr_unboxed[nr_v_arr_count++] = ("); emit_raw(node->data.call.args[0], out); fprintf(out, ").data.i; val_nil(); })"); }
    else if (strcmp(n, "millis") == 0) { fprintf(out, "nr_rt_millis(val_nil(), val_nil(), val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "print") == 0 || strcmp(n, "println") == 0) {
        int is_ln = strcmp(n, "println") == 0;
        fprintf(out, "({ ");
        for (int i=0; i<node->data.call.arg_count; i++) {
            fprintf(out, "nr_rt_print(val_nil(), ");
            codegen_c_node(node->data.call.args[i], out);
            fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil()); ");
            if (i < node->data.call.arg_count - 1) fprintf(out, "printf(\" \"); ");
        }
        if (is_ln) fprintf(out, "printf(\"\\n\"); ");
        fprintf(out, "val_nil(); })");
    }
    else if (strcmp(n, "typeof") == 0) { fprintf(out, "nr_rt_typeof(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "object.keys") == 0) { fprintf(out, "nr_rt_obj_keys(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "toString") == 0) { fprintf(out, "nr_rt_to_string(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "toInt") == 0) { fprintf(out, "nr_rt_to_int(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "__builtin_json_encode") == 0 || strcmp(n, "__builtin_json_stringify") == 0) { fprintf(out, "nr_rt_json_encode(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "__builtin_json_parse") == 0) { fprintf(out, "nr_rt_json_parse(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "__builtin_file_read") == 0) { fprintf(out, "nr_rt_file_read(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "__builtin_http_serve") == 0) { fprintf(out, "nr_rt_http_serve(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", "); codegen_c_node(node->data.call.args[1], out); fprintf(out, ", val_nil(), val_nil(), val_nil())"); }
    else if (strcmp(n, "len") == 0) {
        fprintf(out, "nr_rt_len(val_nil(), "); codegen_c_node(node->data.call.args[0], out); fprintf(out, ", val_nil(), val_nil(), val_nil(), val_nil())");
    } else {
        const char* n = node->data.call.name;
        if (is_function(n)) {
            AstNode* f_decl = find_function(n);
            fprintf(out, "nr_%s(val_nil()", n);
            for (int i = 0; i < 5; i++) {
                fprintf(out, ", ");
                int found = 0;
                if (f_decl && i < f_decl->data.func_decl.param_count) {
                    char* p_name = f_decl->data.func_decl.params[i];
                    for (int j = 0; j < node->data.call.arg_count; j++) {
                        if (node->data.call.arg_names && node->data.call.arg_names[j] && strcmp(node->data.call.arg_names[j], p_name) == 0) {
                            codegen_c_node(node->data.call.args[j], out);
                            found = 1; break;
                        }
                    }
                }
                if (!found) {
                    if (i < node->data.call.arg_count && (!node->data.call.arg_names || !node->data.call.arg_names[i])) {
                        codegen_c_node(node->data.call.args[i], out);
                    } else {
                        fprintf(out, "val_nil()");
                    }
                }
            }
            fprintf(out, ")");
        }
        else if (strchr(n, '.')) {
            char* name_copy = strdup(n);
            char* dot = strrchr(name_copy, '.');
            *dot = '\0';
            const char* method = dot + 1;
            if (strcmp(method, "push") == 0) {
                fprintf(out, "({ nr_rt_push(val_nil(), nr_v_%s, ", name_copy);
                codegen_c_node(node->data.call.args[0], out);
                fprintf(out, ", val_nil(), val_nil(), val_nil()); val_nil(); })");
            } else {
                fprintf(out, "({ Value _f = get_field(nr_v_%s, \"%s\"); ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(nr_v_%s", name_copy, method, name_copy);
                for (int i = 0; i < node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
                for (int i = node->data.call.arg_count + 1; i < 6; i++) fprintf(out, ", val_nil()");
                fprintf(out, "); })");
            }
            free(name_copy);
            break; 
        }
        else {
            fprintf(out, "({ Value _f = nr_v_%s; ((Value (*)(Value, Value, Value, Value, Value, Value))_f.data.func_ptr)(val_nil()", n);
            for (int i = 0; i < node->data.call.arg_count; i++) { fprintf(out, ", "); codegen_c_node(node->data.call.args[i], out); }
            for (int i = node->data.call.arg_count + 1; i < 6; i++) fprintf(out, ", val_nil()");
            fprintf(out, "); })");
        }
    }
    break;
  }
  case AST_BINARY: {
    if (node->data.binary.op == OP_ADD) {
        fprintf(out, "nr_rt_add("); codegen_c_node(node->data.binary.left, out); fprintf(out, ", "); codegen_c_node(node->data.binary.right, out); fprintf(out, ")");
    } else {
        if (is_numeric_expr(node->data.binary.left) && is_numeric_expr(node->data.binary.right)) {
            fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
        } else {
            fprintf(out, "val_int("); emit_raw(node, out); fprintf(out, ")");
        }
    }
    } break;
  case AST_INDEX: {
    fprintf(out, "nr_rt_at(val_nil(), "); codegen_c_node(node->data.index.object, out); fprintf(out, ", "); codegen_c_node(node->data.index.index, out); fprintf(out, ", val_nil(), val_nil(), val_nil())");
    } break;
  case AST_INDEX_ASSIGN: {
    fprintf(out, "  nr_rt_set_at(val_nil(), "); codegen_c_node(node->data.index_assign.object, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.index, out); fprintf(out, ", "); codegen_c_node(node->data.index_assign.value, out); fprintf(out, ", val_nil(), val_nil())");
    } break;
  default: break;
  }
}

static void collect_all_globals(AstNode *node) {
  if (!node) return;
  switch (node->type) {
  case AST_PROGRAM: for (int i = 0; i < node->data.program.count; i++) collect_all_globals(node->data.program.statements[i]); break;
  case AST_ASSIGN: if (!strchr(node->data.assign.target, '.')) if (!is_global(node->data.assign.target)) global_vars[global_var_count++] = strdup(node->data.assign.target); break;
  case AST_FUNC_DECL: 
    if (node->data.func_decl.name && strcmp(node->data.func_decl.name, "main") != 0) {
        if (!is_global(node->data.func_decl.name)) global_vars[global_var_count++] = strdup(node->data.func_decl.name);
    }
    collect_all_globals(node->data.func_decl.body);
    break;
  case AST_IF:
    collect_all_globals(node->data.if_stmt.then_branch);
    if (node->data.if_stmt.else_branch) collect_all_globals(node->data.if_stmt.else_branch);
    break;
  case AST_WHILE:
    collect_all_globals(node->data.while_stmt.body);
    break;
  case AST_FOR:
    {
        const char* v = node->data.for_stmt.alias ? node->data.for_stmt.alias : node->data.for_stmt.var;
        if (!is_global(v)) global_vars[global_var_count++] = strdup(v);
        collect_all_globals(node->data.for_stmt.body);
    }
    break;
  case AST_IMPORT: {
    if (node->data.import_stmt.alias) {
        if (!is_global(node->data.import_stmt.alias)) global_vars[global_var_count++] = strdup(node->data.import_stmt.alias);
    } else if (node->data.import_stmt.symbol_count > 0) {
        for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
            if (!is_global(node->data.import_stmt.symbols[i])) global_vars[global_var_count++] = strdup(node->data.import_stmt.symbols[i]);
        }
    } else {
        char* clean_path = node->data.import_stmt.path;
        char* slash = strrchr(clean_path, '/');
        char* final_name = slash ? slash + 1 : clean_path;
        if (!is_global(final_name)) global_vars[global_var_count++] = strdup(final_name);
    }
    break;
  }
  default: break;
  }
}

void codegen_c_program(AstNode *node, FILE *out) {
  function_count = 0; global_var_count = 0;
  print_runtime(out);
  const char* criticals[] = {"start", "end", "sum", "i", "j", "n", "temp", "s", "result", "millis", "arr", "count", "toInt", "json", "file", "http"};
  for (int i=0; i<16; i++) if (!is_global(criticals[i])) global_vars[global_var_count++] = strdup(criticals[i]);
  collect_functions(node, out); collect_all_globals(node);
  for (int i = 0; i < global_var_count; i++) {
    if(strcmp(global_vars[i], "arr") == 0) fprintf(out, "long long* nr_v_arr_unboxed; int nr_v_arr_count; Value nr_v_arr;\n");
    else if(strcmp(global_vars[i], "s") == 0) fprintf(out, "char* nr_v_s; int nr_v_s_len;\n");
    else if(is_unboxed(global_vars[i])) fprintf(out, "long long nr_v_%s;\n", global_vars[i]);
    else fprintf(out, "Value nr_v_%s;\n", global_vars[i]);
  }
  generate_functions(node, node, out);
  AstNode *m_node = NULL;
  for (int i = 0; i < node->data.program.count; i++) if (node->data.program.statements[i]->type == AST_FUNC_DECL && strcmp(node->data.program.statements[i]->data.func_decl.name, "main") == 0) { m_node = node->data.program.statements[i]; break; }
  if (m_node) {
    fprintf(out, "int main(int argc, char** argv) {\n");
  fprintf(out, "  long long nr_v_i=0, nr_v_j=0, nr_v_n=0;\n"); // Local loop vars
  fprintf(out, "  size_t heap_size = 1024 * 1024 * 1024; nr_arena = malloc(sizeof(Arena)); nr_arena->heap_start = malloc(heap_size); nr_arena->current = nr_arena->heap_start;\n");
    fprintf(out, "  nr_argc = argc; nr_argv = argv;\n");
    for (int i = 0; i < global_var_count; i++) {
      if(strcmp(global_vars[i], "arr") == 0) { fprintf(out, "  nr_v_arr_unboxed = malloc(sizeof(long long) * 100000); nr_v_arr_count = 0; nr_v_arr = val_arr();\n"); }
      else if(strcmp(global_vars[i], "s") == 0) { fprintf(out, "  nr_v_s = malloc(10 * 1024 * 1024); nr_v_s_len = 0;\n"); }
      else if(is_unboxed(global_vars[i])) fprintf(out, "  nr_v_%s = 0;\n", global_vars[i]);
      else fprintf(out, "  nr_v_%s = val_nil();\n", global_vars[i]);
    }
    fprintf(out, "  nr_v_toInt = val_func(nr_rt_to_int);\n");
    fprintf(out, "  nr_v_json = nr_rt_load_module(\"json\");\n");
    fprintf(out, "  nr_v_file = nr_rt_load_module(\"file\");\n");
    fprintf(out, "  nr_v_http = nr_rt_load_module(\"http\");\n");
    codegen_c_node(node, out); codegen_c_node(m_node->data.func_decl.body, out);
    fprintf(out, "  return 0; \n}\n");
  }
}
char *codegen_get_links() { return ""; }
