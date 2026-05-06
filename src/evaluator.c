#include "../include/evaluator.h"
#include "../include/parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <errno.h>

void set_field(Object* obj, const char* key, Value val) {
    for (int i=0; i<obj->count; i++) {
        if (strcmp(obj->keys[i], key) == 0) {
            *obj->values[i] = val;
            return;
        }
    }
    if (obj->count >= obj->capacity) {
        obj->capacity *= 2;
        obj->keys = realloc(obj->keys, sizeof(char*) * obj->capacity);
        obj->values = realloc(obj->values, sizeof(Value*) * obj->capacity);
    }
    obj->keys[obj->count] = strdup(key);
    obj->values[obj->count] = malloc(sizeof(Value));
    *obj->values[obj->count] = val;
    obj->count++;
}

// --- Include Paths ---
static char* include_paths[16];
static int include_path_count = 0;

void nr_eval_add_include_path(const char* path) {
    if (include_path_count < 16) {
        include_paths[include_path_count++] = strdup(path);
    }
}

// --- Value Constructors ---

Value val_int(int i) { return (Value){.type = VAL_INT, .data.i = i}; }
Value val_str(char* s) { return (Value){.type = VAL_STR, .data.s = s ? strdup(s) : strdup("")}; }
Value val_nil() { return (Value){.type = VAL_NIL}; }
Value val_bool(int b) { return (Value){.type = VAL_BOOL, .data.i = b}; }
Value val_error(char* msg) { return (Value){.type = VAL_ERROR, .data.s = msg ? strdup(msg) : strdup("error")}; }

Value val_return(Value v) {
    Value res;
    res.type = VAL_RETURN;
    res.data.return_val = malloc(sizeof(Value));
    *res.data.return_val = v;
    return res;
}

Value val_obj() {
    Value v;
    v.type = VAL_OBJ;
    v.data.obj = malloc(sizeof(Object));
    v.data.obj->count = 0;
    v.data.obj->capacity = 16;
    v.data.obj->keys = malloc(sizeof(char*) * 16);
    v.data.obj->values = malloc(sizeof(Value*) * 16);
    return v;
}

Value val_arr() {
    Value v;
    v.type = VAL_ARR;
    v.data.arr = malloc(sizeof(Array));
    v.data.arr->count = 0;
    v.data.arr->capacity = 16;
    v.data.arr->elements = malloc(sizeof(Value*) * 16);
    return v;
}

Value val_func(AstNode* decl, Environment* closure) {
    return (Value){.type = VAL_FUNC, .data.func.decl = decl, .data.func.closure = closure};
}

// --- Environment ---

Environment* env_new(Environment* parent) {
    Environment* env = malloc(sizeof(Environment));
    env->vars = NULL;
    env->parent = parent;
    env->source = parent ? parent->source : NULL;
    env->filename = parent ? parent->filename : NULL;
    return env;
}

void env_define(Environment* env, char* name, Value val) {
    if (!name) return;
    Variable* v = malloc(sizeof(Variable));
    v->name = strdup(name);
    v->value = val;
    v->next = env->vars;
    env->vars = v;
}

void env_assign(Environment* env, char* name, Value val) {
    if (!name) return;
    Environment* curr = env;
    while (curr) {
        Variable* v = curr->vars;
        while (v) {
            if (strcmp(v->name, name) == 0) {
                v->value = val;
                return;
            }
            v = v->next;
        }
        curr = curr->parent;
    }
    env_define(env, name, val);
}

Value env_get(Environment* env, char* name) {
    if (!name) return val_nil();
    Environment* curr = env;
    while (curr) {
        Variable* v = curr->vars;
        while (v) {
            if (v->name && strcmp(v->name, name) == 0) return v->value;
            v = v->next;
        }
        curr = curr->parent;
    }
    return val_nil();
}

// --- Diagnostics ---

static void report_runtime_error(AstNode* node, Environment* env, const char* name, const char* msg) {
    fprintf(stderr, "\n\033[1;31m[%s ERROR]\033[0m %s\n", name, msg);
    fprintf(stderr, "\033[1;34m-->\033[0m %s:%d:%d\n", env->filename ? env->filename : "source.nr", node->line, node->column);
    
    const char* source = NULL;
    Environment* curr = env;
    while (curr) {
        if (curr->source) { source = curr->source; break; }
        curr = curr->parent;
    }

    if (source) {
        int line = 1;
        int i = 0;
        while (line < node->line && source[i] != '\0') {
            if (source[i] == '\n') line++;
            i++;
        }
        const char* start = source + i;
        const char* end = start;
        while (*end != '\n' && *end != '\0') end++;
        
        fprintf(stderr, " \033[1;34m|\033[0m\n");
        fprintf(stderr, " \033[1;34m| \033[0m %.*s\n", (int)(end - start), start);
        fprintf(stderr, " \033[1;34m| \033[1;31m");
        for (int j = 1; j < node->column; j++) fprintf(stderr, " ");
        fprintf(stderr, "^\033[0m\n");
        fprintf(stderr, " \033[1;34m|\033[0m\n\n");
    }
    exit(1);
}

// --- Helpers ---

static char* read_file_internal(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    char* buffer = (char*)malloc(fileSize + 1);
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

char* val_to_json_internal(Value v) {
    if (v.type == VAL_INT) {
        char* b = malloc(32); sprintf(b, "%d", v.data.i); return b;
    }
    if (v.type == VAL_STR) {
        char* b = malloc(strlen(v.data.s) + 3); sprintf(b, "\"%s\"", v.data.s); return b;
    }
    if (v.type == VAL_BOOL) return strdup(v.data.i ? "true" : "false");
    if (v.type == VAL_ARR) {
        char* res = strdup("[");
        for (int i=0; i<v.data.arr->count; i++) {
            char* item = val_to_json_internal(*v.data.arr->elements[i]);
            char* old = res;
            res = malloc(strlen(old) + strlen(item) + 3);
            sprintf(res, "%s%s%s", old, item, i < v.data.arr->count-1 ? "," : "");
            free(old); free(item);
        }
        char* old = res; res = malloc(strlen(old) + 2); sprintf(res, "%s]", old); free(old);
        return res;
    }
    if (v.type == VAL_OBJ) {
        char* res = strdup("{");
        for (int i=0; i<v.data.obj->count; i++) {
            char* val = val_to_json_internal(*v.data.obj->values[i]);
            char* old = res;
            res = malloc(strlen(old) + strlen(v.data.obj->keys[i]) + strlen(val) + 6);
            sprintf(res, "%s\"%s\":%s%s", old, v.data.obj->keys[i], val, i < v.data.obj->count-1 ? "," : "");
            free(old); free(val);
        }
        char* old = res; res = malloc(strlen(old) + 2); sprintf(res, "%s}", old); free(old);
        return res;
    }
    return strdup("null");
}

static Value get_dot_value(Value obj, char* field) {
    if (obj.type == VAL_ARR) {
        if (strcmp(field, "length") == 0) return val_int(obj.data.arr->count);
    }
    if (obj.type == VAL_STR) {
        if (strcmp(field, "length") == 0) return val_int(strlen(obj.data.s));
    }
    if (obj.type != VAL_OBJ || !field) return val_nil();
    for (int i=0; i<obj.data.obj->count; i++) {
        if (obj.data.obj->keys[i] && strcmp(obj.data.obj->keys[i], field) == 0) {
            return *obj.data.obj->values[i];
        }
    }
    return val_nil();
}

static int is_truthy(Value v) {
    if (v.type == VAL_NIL) return 0;
    if (v.type == VAL_BOOL) return v.data.i;
    if (v.type == VAL_INT) return v.data.i != 0;
    return 1;
}

// --- Evaluator ---

static Value eval_binary(AstNode* node, Environment* env) {
    Value left = eval(node->data.binary.left, env);
    if (left.type == VAL_RETURN || left.type == VAL_BREAK || left.type == VAL_CONTINUE) return left;
    
    char* op = node->data.binary.op;
    if (!op) return val_nil();

    if (strcmp(op, "and") == 0) {
        if (!is_truthy(left)) return left;
        return eval(node->data.binary.right, env);
    }
    if (strcmp(op, "or") == 0) {
        if (is_truthy(left)) return left;
        return eval(node->data.binary.right, env);
    }
    if (strcmp(op, "not") == 0) {
        return val_bool(!is_truthy(left));
    }

    Value right = eval(node->data.binary.right, env);
    if (right.type == VAL_RETURN || right.type == VAL_BREAK || right.type == VAL_CONTINUE) return right;

    if (left.type == VAL_INT && right.type == VAL_INT) {
        if (strcmp(op, "+") == 0) return val_int(left.data.i + right.data.i);
        if (strcmp(op, "-") == 0) return val_int(left.data.i - right.data.i);
        if (strcmp(op, "*") == 0) return val_int(left.data.i * right.data.i);
        if (strcmp(op, "/") == 0) {
            if (right.data.i == 0) report_runtime_error(node, env, "MATH", "Division by zero");
            return val_int(left.data.i / right.data.i);
        }
        if (strcmp(op, ">") == 0) return val_bool(left.data.i > right.data.i);
        if (strcmp(op, "<") == 0) return val_bool(left.data.i < right.data.i);
        if (strcmp(op, ">=") == 0) return val_bool(left.data.i >= right.data.i);
        if (strcmp(op, "<=") == 0) return val_bool(left.data.i <= right.data.i);
    }
    
    if (strcmp(op, "==") == 0) {
        if (left.type != right.type) return val_bool(0);
        if (left.type == VAL_INT) return val_bool(left.data.i == right.data.i);
        if (left.type == VAL_STR) return val_bool(strcmp(left.data.s, right.data.s) == 0);
        if (left.type == VAL_NIL) return val_bool(1);
        return val_bool(0);
    }
    if (strcmp(op, "!=") == 0) {
        if (left.type != right.type) return val_bool(1);
        if (left.type == VAL_INT) return val_bool(left.data.i != right.data.i);
        if (left.type == VAL_STR) return val_bool(strcmp(left.data.s, right.data.s) != 0);
        if (left.type == VAL_NIL) return val_bool(0);
        return val_bool(1);
    }
    
    if (left.type == VAL_STR && strcmp(op, "+") == 0) {
        char buf[64];
        char* right_s = NULL;
        if (right.type == VAL_STR) right_s = strdup(right.data.s);
        else if (right.type == VAL_INT) { snprintf(buf, sizeof(buf), "%d", right.data.i); right_s = strdup(buf); }
        else if (right.type == VAL_BOOL) right_s = strdup(right.data.i ? "true" : "false");
        else if (right.type == VAL_OBJ) right_s = strdup("[Object]");
        else if (right.type == VAL_ARR) right_s = strdup("[Array]");
        else right_s = strdup("nil");
        
        char* res = malloc(strlen(left.data.s) + strlen(right_s) + 1);
        strcpy(res, left.data.s);
        strcat(res, right_s);
        Value v = val_str(res);
        free(res);
        free(right_s);
        return v;
    }

    return val_nil();
}

static Value eval_call(AstNode* node, Environment* env) {
    if (!node->data.call.name) return val_nil();
    char* full_name = strdup(node->data.call.name);
    char* dot = strchr(full_name, '.');
    Value func_val = val_nil();
    
    if (dot) {
        *dot = '\0';
        char* obj_name = full_name;
        char* field_name = dot + 1;
        Value obj = env_get(env, obj_name);
        
        // Handle Array methods
        if (obj.type == VAL_ARR) {
            if (strcmp(field_name, "push") == 0) {
                if (node->data.call.arg_count > 0) {
                    Value val = eval(node->data.call.args[0], env);
                    if (val.type == VAL_RETURN) val = *val.data.return_val;
                    Array* arr = obj.data.arr;
                    if (arr->count >= arr->capacity) {
                        arr->capacity *= 2;
                        arr->elements = realloc(arr->elements, sizeof(Value*) * arr->capacity);
                    }
                    arr->elements[arr->count] = malloc(sizeof(Value));
                    *arr->elements[arr->count] = val;
                    arr->count++;
                    free(full_name);
                    return val;
                }
            } else if (strcmp(field_name, "pop") == 0) {
                Array* arr = obj.data.arr;
                if (arr->count > 0) {
                    arr->count--;
                    Value v = *arr->elements[arr->count];
                    // free(arr->elements[arr->count]); // Value might be used? Actually val_free handles it
                    free(full_name);
                    return v;
                }
                free(full_name);
                return val_nil();
            } else if (strcmp(field_name, "length") == 0) {
                free(full_name);
                return val_int(obj.data.arr->count);
            }
        }
        
        // Handle String methods
        if (obj.type == VAL_STR) {
            if (strcmp(field_name, "length") == 0) {
                free(full_name);
                return val_int(strlen(obj.data.s));
            } else if (strcmp(field_name, "substring") == 0) {
                if (node->data.call.arg_count >= 2) {
                    Value start = eval(node->data.call.args[0], env);
                    Value len = eval(node->data.call.args[1], env);
                    if (start.type == VAL_RETURN) start = *start.data.return_val;
                    if (len.type == VAL_RETURN) len = *len.data.return_val;
                    
                    if (start.type == VAL_INT && len.type == VAL_INT) {
                        int slen = strlen(obj.data.s);
                        int istart = start.data.i;
                        int ilen = len.data.i;
                        if (istart < 0) istart = 0;
                        if (istart >= slen) { free(full_name); return val_str(strdup("")); }
                        if (istart + ilen > slen) ilen = slen - istart;
                        char* sub = malloc(ilen + 1);
                        strncpy(sub, obj.data.s + istart, ilen);
                        sub[ilen] = 0;
                        free(full_name);
                        return val_str(sub);
                    }
                }
            }
        }

        func_val = get_dot_value(obj, field_name);
        
        if (func_val.type == VAL_FUNC || func_val.type == VAL_NIL) {
            if (func_val.type == VAL_NIL) {
                func_val = env_get(env, field_name);
            }
            
            if (func_val.type == VAL_FUNC) {
                AstNode* decl = func_val.data.func.decl;
                Environment* call_env = env_new(func_val.data.func.closure);
                env_define(call_env, "self", obj);
                
                // Normal argument binding for methods
                for (int i=0; i<node->data.call.arg_count && i < decl->data.func_decl.param_count; i++) {
                    Value arg = eval(node->data.call.args[i], env);
                    if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
                    env_define(call_env, decl->data.func_decl.params[i], arg);
                }
                Value res = eval(decl->data.func_decl.body, call_env);
                free(full_name);
                if (res.type == VAL_RETURN) return *res.data.return_val;
                return res;
            }
        }
    } else {
        // Built-ins
        if (strcmp(full_name, "print") == 0) {
            for (int i=0; i<node->data.call.arg_count; i++) {
                Value arg = eval(node->data.call.args[i], env);
                if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
                if (arg.type == VAL_INT) printf("%d\n", arg.data.i);
                else if (arg.type == VAL_BOOL) printf("%s\n", arg.data.i ? "true" : "false");
                else if (arg.type == VAL_STR) printf("%s\n", arg.data.s);
                else if (arg.type == VAL_OBJ) printf("[Object]\n");
                else if (arg.type == VAL_ARR) printf("[Array]\n");
                else if (arg.type == VAL_ERROR) printf("Error: %s\n", arg.data.s);
                else printf("nil\n");
            }
            fflush(stdout);
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_len") == 0) {
            Value arg = eval(node->data.call.args[0], env);
            if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
            int len = 0;
            if (arg.type == VAL_ARR) len = arg.data.arr->count;
            else if (arg.type == VAL_STR) len = strlen(arg.data.s);
            free(full_name);
            return val_int(len);
        }
        if (strcmp(full_name, "__builtin_file_write") == 0) {
            Value path = eval(node->data.call.args[0], env);
            Value content = eval(node->data.call.args[1], env);
            if (path.type == VAL_STR && content.type == VAL_STR) {
                FILE* f = fopen(path.data.s, "w");
                if (f) {
                    fputs(content.data.s, f);
                    fclose(f);
                }
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_file_read") == 0) {
            Value path = eval(node->data.call.args[0], env);
            Value res = val_nil();
            if (path.type == VAL_STR) {
                char* s = read_file_internal(path.data.s);
                if (s) {
                    res = val_str(s);
                    free(s);
                }
            }
            free(full_name);
            return res;
        }
        if (strcmp(full_name, "__builtin_exec") == 0) {
            Value cmd = eval(node->data.call.args[0], env);
            if (cmd.type == VAL_STR) {
                system(cmd.data.s);
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_random") == 0) {
            static int seeded = 0;
            if (!seeded) { srand(time(NULL)); seeded = 1; }
            free(full_name);
            return val_int(rand());
        }
        if (strcmp(full_name, "__builtin_time_now") == 0) {
            free(full_name);
            return val_int((int)time(NULL));
        }
        if (strcmp(full_name, "__builtin_db_open") == 0) {
            free(full_name);
            return eval(node->data.call.args[0], env);
        }
        if (strcmp(full_name, "__builtin_db_exec") == 0) {
            Value path = eval(node->data.call.args[0], env);
            Value sql = eval(node->data.call.args[1], env);
            Value params = eval(node->data.call.args[2], env);
            if (path.type == VAL_STR && sql.type == VAL_STR) {
                sqlite3* db; if (sqlite3_open(path.data.s, &db) == SQLITE_OK) {
                    sqlite3_stmt* stmt;
                    if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
                        if (params.type == VAL_ARR) {
                            for (int i=0; i<params.data.arr->count; i++) {
                                Value v = *params.data.arr->elements[i];
                                if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, i+1, v.data.i);
                                else if (v.type == VAL_STR) sqlite3_bind_text(stmt, i+1, v.data.s, -1, SQLITE_TRANSIENT);
                                else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, i+1);
                            }
                        }
                        sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(db);
                }
            }
            free(full_name);
            return val_nil();
        }
        // SQLite query execution using prepare/step
        if (strcmp(full_name, "__builtin_db_query") == 0) {
            Value path = eval(node->data.call.args[0], env);
            Value sql = eval(node->data.call.args[1], env);
            Value params = eval(node->data.call.args[2], env);
            Value a = val_arr();
            if (path.type == VAL_STR && sql.type == VAL_STR) {
                sqlite3* db; if (sqlite3_open(path.data.s, &db) == SQLITE_OK) {
                    sqlite3_stmt* stmt;
                    if (sqlite3_prepare_v2(db, sql.data.s, -1, &stmt, 0) == SQLITE_OK) {
                        if (params.type == VAL_ARR) {
                            for (int i=0; i<params.data.arr->count; i++) {
                                Value v = *params.data.arr->elements[i];
                                if (v.type == VAL_INT || v.type == VAL_BOOL) sqlite3_bind_int(stmt, i+1, v.data.i);
                                else if (v.type == VAL_STR) sqlite3_bind_text(stmt, i+1, v.data.s, -1, SQLITE_TRANSIENT);
                                else if (v.type == VAL_NIL) sqlite3_bind_null(stmt, i+1);
                            }
                        }
                        int cols = sqlite3_column_count(stmt);
                        while (sqlite3_step(stmt) == SQLITE_ROW) {
                            Value o = val_obj();
                            for (int i=0; i<cols; i++) {
                                const char* name = sqlite3_column_name(stmt, i);
                                const char* text = (const char*)sqlite3_column_text(stmt, i);
                                // Set field in Nira object
                                if (o.data.obj->count >= o.data.obj->capacity) {
                                    o.data.obj->capacity *= 2;
                                    o.data.obj->keys = realloc(o.data.obj->keys, sizeof(char*) * o.data.obj->capacity);
                                    o.data.obj->values = realloc(o.data.obj->values, sizeof(Value*) * o.data.obj->capacity);
                                }
                                o.data.obj->keys[o.data.obj->count] = strdup(name);
                                o.data.obj->values[o.data.obj->count] = malloc(sizeof(Value));
                                *o.data.obj->values[o.data.obj->count] = val_str((char*)text);
                                o.data.obj->count++;
                            }
                            if (a.data.arr->count >= a.data.arr->capacity) {
                                a.data.arr->capacity *= 2;
                                a.data.arr->elements = realloc(a.data.arr->elements, sizeof(Value*) * a.data.arr->capacity);
                            }
                            a.data.arr->elements[a.data.arr->count] = malloc(sizeof(Value));
                            *a.data.arr->elements[a.data.arr->count] = o;
                            a.data.arr->count++;
                        }
                        sqlite3_finalize(stmt);
                    }
                    sqlite3_close(db);
                }
            }
            free(full_name);
            return a;
        }
        if (strcmp(full_name, "__builtin_delay") == 0) {
            Value ms = eval(node->data.call.args[0], env);
            if (ms.type == VAL_INT) {
                usleep(ms.data.i * 1000);
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_exit_proc") == 0) {
            Value code = eval(node->data.call.args[0], env);
            int c = (code.type == VAL_INT) ? code.data.i : 0;
            free(full_name);
            exit(c);
        }
        if (strcmp(full_name, "__builtin_file_exists") == 0) {
            Value path = eval(node->data.call.args[0], env);
            int res = 0;
            if (path.type == VAL_STR) {
                struct stat buffer;
                res = (stat(path.data.s, &buffer) == 0);
            }
            free(full_name);
            return val_bool(res);
        }
        if (strcmp(full_name, "__builtin_str_index_of") == 0) {
            Value s = eval(node->data.call.args[0], env);
            Value sub = eval(node->data.call.args[1], env);
            int res = -1;
            if (s.type == VAL_STR && sub.type == VAL_STR) {
                char* p = strstr(s.data.s, sub.data.s);
                if (p) res = (int)(p - s.data.s);
            }
            free(full_name);
            return val_int(res);
        }
        if (strcmp(full_name, "__builtin_str_replace") == 0) {
            Value s = eval(node->data.call.args[0], env);
            Value old = eval(node->data.call.args[1], env);
            Value new_str = eval(node->data.call.args[2], env);
            Value res = s;
            if (s.type == VAL_STR && old.type == VAL_STR && new_str.type == VAL_STR) {
                int oldlen = strlen(old.data.s);
                if (oldlen > 0) {
                    int count = 0;
                    char *p = s.data.s;
                    while ((p = strstr(p, old.data.s))) { count++; p += oldlen; }
                    char* result = malloc(strlen(s.data.s) + count * (strlen(new_str.data.s) - oldlen) + 1);
                    char* d = result;
                    p = s.data.s;
                    while (*p) {
                        if (strstr(p, old.data.s) == p) {
                            strcpy(d, new_str.data.s);
                            d += strlen(new_str.data.s);
                            p += oldlen;
                        } else *d++ = *p++;
                    }
                    *d = 0;
                    res = val_str(result);
                    free(result);
                }
            }
            free(full_name);
            return res;
        }
        if (strcmp(full_name, "__builtin_str_match") == 0) {
            Value s = eval(node->data.call.args[0], env);
            Value regex_str = eval(node->data.call.args[1], env);
            int res = 0;
            if (s.type == VAL_STR && regex_str.type == VAL_STR) {
                #include <regex.h>
                regex_t regex;
                if (regcomp(&regex, regex_str.data.s, REG_EXTENDED) == 0) {
                    if (regexec(&regex, s.data.s, 0, NULL, 0) == 0) res = 1;
                    regfree(&regex);
                }
            }
            free(full_name);
            return val_bool(res);
        }
        if (strcmp(full_name, "__builtin_file_delete") == 0) {
            Value path = eval(node->data.call.args[0], env);
            if (path.type == VAL_STR) {
                remove(path.data.s);
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_substring") == 0) {
            Value s = eval(node->data.call.args[0], env);
            Value start = eval(node->data.call.args[1], env);
            Value len = eval(node->data.call.args[2], env);
            if (s.type == VAL_STR && start.type == VAL_INT && len.type == VAL_INT) {
                int slen = strlen(s.data.s);
                int st = start.data.i;
                int l = len.data.i;
                if (st < 0) st = 0;
                if (st > slen) st = slen;
                if (st + l > slen) l = slen - st;
                char* sub = malloc(l + 1);
                strncpy(sub, s.data.s + st, l);
                sub[l] = '\0';
                Value res = val_str(sub);
                free(sub);
                free(full_name);
                return res;
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "__builtin_push") == 0) {
            Value arr = eval(node->data.call.args[0], env);
            Value val = eval(node->data.call.args[1], env);
            if (arr.type == VAL_ARR) {
                if (arr.data.arr->count >= arr.data.arr->capacity) {
                    arr.data.arr->capacity *= 2;
                    arr.data.arr->elements = realloc(arr.data.arr->elements, sizeof(Value*) * arr.data.arr->capacity);
                }
                arr.data.arr->elements[arr.data.arr->count] = malloc(sizeof(Value));
                *arr.data.arr->elements[arr.data.arr->count] = val;
                arr.data.arr->count++;
            }
            free(full_name);
            return val;
        }
        if (strcmp(full_name, "__builtin_pop") == 0) {
            Value arr = eval(node->data.call.args[0], env);
            Value res = val_nil();
            if (arr.type == VAL_ARR && arr.data.arr->count > 0) {
                arr.data.arr->count--;
                res = *arr.data.arr->elements[arr.data.arr->count];
            }
            free(full_name);
            return res;
        }
        if (strcmp(full_name, "__builtin_http_serve") == 0) {
            Value port = eval(node->data.call.args[0], env);
            Value callback = eval(node->data.call.args[1], env);
            if (port.type == VAL_INT && callback.type == VAL_FUNC) {
                int server_fd, new_socket;
                struct sockaddr_in address;
                int addrlen = sizeof(address);
                server_fd = socket(AF_INET, SOCK_STREAM, 0);
                int opt = 1;
                setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = INADDR_ANY;
                address.sin_port = htons(port.data.i);
                if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
                    char err_msg[256];
                    snprintf(err_msg, sizeof(err_msg), "Could not bind to port %d: %s", port.data.i, strerror(errno));
                    report_runtime_error(node, env, "NETWORK", err_msg);
                    free(full_name);
                    return val_nil();
                }
                if (listen(server_fd, 3) < 0) {
                    report_runtime_error(node, env, "NETWORK", "Could not listen on port");
                    free(full_name);
                    return val_nil();
                }
                printf("HTTP Server listening on port %d...\n", port.data.i);
                while(1) {
                    new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
                    if (new_socket < 0) continue;

                    char buffer[4096] = {0};
                    int bytes_read = read(new_socket, buffer, 4096);
                    if (bytes_read <= 0) {
                        close(new_socket);
                        continue;
                    }
                    
                    // Simple parse
                    char method[16] = {0}, path[256] = {0};
                    if (sscanf(buffer, "%s %s", method, path) < 2) {
                        close(new_socket);
                        continue;
                    }
                    
                    Value req = val_obj();
                    set_field(req.data.obj, "method", val_str(strdup(method)));
                    set_field(req.data.obj, "path", val_str(strdup(path)));
                    
                    char* body_ptr = strstr(buffer, "\r\n\r\n");
                    if (body_ptr) {
                        body_ptr += 4;
                        set_field(req.data.obj, "body", val_str(strdup(body_ptr)));
                    } else {
                        set_field(req.data.obj, "body", val_nil());
                    }
                    
                    // Call Nira serve function
                    Environment* call_env = env_new(callback.data.func.closure);
                    env_define(call_env, "req", req);
                    Value res = eval(callback.data.func.decl->data.func_decl.body, call_env);
                    if (res.type == VAL_RETURN) res = *res.data.return_val;
                    
                    char* body = "Not Found";
                    int status = 404;
                    char headers_str[1024] = {0};
                    if (res.type == VAL_OBJ) {
                        Value s = get_dot_value(res, "status");
                        if (s.type == VAL_INT) status = s.data.i;
                        Value b = get_dot_value(res, "body");
                        if (b.type == VAL_STR) body = b.data.s;
                        
                        Value h = get_dot_value(res, "headers");
                        if (h.type == VAL_OBJ) {
                            for (int i=0; i<h.data.obj->count; i++) {
                                char line[256];
                                Value* hv = h.data.obj->values[i];
                                char* hs = "";
                                if (hv->type == VAL_STR) hs = hv->data.s;
                                else if (hv->type == VAL_INT) { sprintf(line, "%d", hv->data.i); hs = strdup(line); }
                                
                                sprintf(line, "%s: %s\r\n", h.data.obj->keys[i], hs);
                                strcat(headers_str, line);
                            }
                        }
                    }
                    
                    char resp[8192];
                    sprintf(resp, "HTTP/1.1 %d OK\r\n%sContent-Length: %zu\r\n\r\n%s", status, headers_str, strlen(body), body);
                    send(new_socket, resp, strlen(resp), 0);
                    close(new_socket);
                }
            }
            free(full_name);
            return val_nil();
        }
        if (strcmp(full_name, "toInt") == 0) {
            Value v = eval(node->data.call.args[0], env);
            if (v.type == VAL_RETURN) v = *v.data.return_val;
            int res = 0;
            if (v.type == VAL_INT) res = v.data.i;
            else if (v.type == VAL_STR) res = atoi(v.data.s);
            free(full_name);
            return val_int(res);
        }
        if (strcmp(full_name, "len") == 0 || strcmp(full_name, "__builtin_len") == 0) {
            Value v = eval(node->data.call.args[0], env);
            if (v.type == VAL_RETURN) v = *v.data.return_val;
            int res = 0;
            if (v.type == VAL_STR) res = strlen(v.data.s);
            else if (v.type == VAL_ARR) res = v.data.arr->count;
            else if (v.type == VAL_OBJ) res = v.data.obj->count;
            free(full_name);
            return val_int(res);
        }
        if (strcmp(full_name, "__builtin_json_encode") == 0 || strcmp(full_name, "json_encode") == 0) {
            Value v = eval(node->data.call.args[0], env);
            if (v.type == VAL_RETURN) v = *v.data.return_val;
            char* s = val_to_json_internal(v);
            Value res = val_str(s);
            free(s);
            free(full_name);
            return res;
        }
        if (strcmp(full_name, "__builtin_json_decode") == 0 || strcmp(full_name, "json_decode") == 0) {
            Value v = eval(node->data.call.args[0], env);
            if (v.type == VAL_RETURN) v = *v.data.return_val;
            if (v.type != VAL_STR) { free(full_name); return val_nil(); }
            
            char* p = v.data.s; 
            while(*p == ' ' || *p == '[' || *p == ']') p++;
            if (*p == '{') {
                Value obj = val_obj(); p++;
                while(*p && *p != '}') {
                    while(*p == ' ' || *p == '"' || *p == ',') p++;
                    char key[64]; int i=0; while(*p && *p != '"') key[i++] = *p++; key[i] = 0; p++;
                    while(*p == ' ' || *p == ':') p++;
                    if (*p == '"') { 
                        p++; char val[256]; int j=0; while(*p && *p != '"') val[j++] = *p++; val[j] = 0; p++; 
                        set_field(obj.data.obj, key, val_str(strdup(val))); 
                    } else { 
                        char val[32]; int j=0; while(*p && *p != ',' && *p != ' ' && *p != '}') val[j++] = *p++; val[j] = 0; 
                        set_field(obj.data.obj, key, val_int(atoi(val))); 
                    }
                }
                free(full_name);
                return obj;
            }
            free(full_name);
            return val_nil();
        }
        func_val = env_get(env, full_name);
    }

    if (func_val.type != VAL_FUNC) {
        char buf[256];
        snprintf(buf, sizeof(buf), "'%s' is not a function", node->data.call.name);
        report_runtime_error(node, env, "TYPE", buf);
    }

    AstNode* decl = func_val.data.func.decl;
    Environment* call_env = env_new(func_val.data.func.closure);

    // Bind arguments
    if (decl->data.func_decl.is_unpacking) {
        Value data = eval(node->data.call.args[0], env);
        if (data.type == VAL_RETURN) data = *data.data.return_val;
        for (int i=0; i<decl->data.func_decl.param_count; i++) {
            char* key = decl->data.func_decl.params[i];
            Value val = val_nil();
            if (data.type == VAL_OBJ) {
                for (int j=0; j<data.data.obj->count; j++) {
                    if (strcmp(data.data.obj->keys[j], key) == 0) {
                        val = *data.data.obj->values[j];
                        break;
                    }
                }
            }
            env_define(call_env, key, val);
        }
    } else {
        for (int i=0; i<node->data.call.arg_count && i<decl->data.func_decl.param_count; i++) {
            Value arg = eval(node->data.call.args[i], env);
            if (arg.type == VAL_RETURN) arg = *arg.data.return_val;
            env_define(call_env, decl->data.func_decl.params[i], arg);
        }
    }

    Value result = eval(decl->data.func_decl.body, call_env);
    free(full_name);
    if (result.type == VAL_RETURN) return *result.data.return_val;
    return result;
}

Value eval(AstNode* node, Environment* env) {
    if (!node) return val_nil();

    switch (node->type) {
        case AST_PROGRAM: {
            Value last = val_nil();
            for (int i=0; i<node->data.program.count; i++) {
                last = eval(node->data.program.statements[i], env);
                if (last.type == VAL_RETURN || last.type == VAL_BREAK || last.type == VAL_CONTINUE) return last;
            }
            return last;
        }
        case AST_FUNC_DECL: {
            Value f = val_func(node, env);
            if (strcmp(node->data.func_decl.name, "anonymous") != 0) {
                env_define(env, node->data.func_decl.name, f);
            }
            return f;
        }
        case AST_ASSIGN: {
            Value v = eval(node->data.assign.value, env);
            if (v.type == VAL_RETURN) v = *v.data.return_val;
            
            char* target = strdup(node->data.assign.target);
            char* dot = strchr(target, '.');
            if (dot) {
                *dot = '\0';
                char* obj_name = target;
                char* field_name = dot + 1;
                Value obj = env_get(env, obj_name);
                if (obj.type == VAL_OBJ) {
                    int found = 0;
                    for (int i=0; i<obj.data.obj->count; i++) {
                        if (strcmp(obj.data.obj->keys[i], field_name) == 0) {
                            *obj.data.obj->values[i] = v;
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        if (obj.data.obj->count >= obj.data.obj->capacity) {
                            obj.data.obj->capacity *= 2;
                            obj.data.obj->keys = realloc(obj.data.obj->keys, sizeof(char*) * obj.data.obj->capacity);
                            obj.data.obj->values = realloc(obj.data.obj->values, sizeof(Value*) * obj.data.obj->capacity);
                        }
                        obj.data.obj->keys[obj.data.obj->count] = strdup(field_name);
                        obj.data.obj->values[obj.data.obj->count] = malloc(sizeof(Value));
                        *obj.data.obj->values[obj.data.obj->count] = v;
                        obj.data.obj->count++;
                    }
                }
            } else {
                env_assign(env, target, v);
            }
            free(target);
            return v;
        }
        case AST_VAR_REF: {
            char* full_name = strdup(node->data.var_name);
            char* dot = strchr(full_name, '.');
            Value v = val_nil();
            if (dot) {
                *dot = '\0';
                Value obj = env_get(env, full_name);
                v = get_dot_value(obj, dot + 1);
            } else {
                v = env_get(env, full_name);
            }
            free(full_name);
            return v;
        }
        case AST_LITERAL_INT: return val_int(node->data.int_val);
        case AST_LITERAL_STR: return val_str(node->data.str_val);
        case AST_LITERAL_BOOL: return val_bool(node->data.int_val);
        case AST_LITERAL_NULL: return val_nil();
        case AST_BINARY: return eval_binary(node, env);
        case AST_CALL: return eval_call(node, env);
        case AST_RETURN: {
            Value v = eval(node->data.ret.value, env);
            if (v.type == VAL_RETURN) return v;
            return val_return(v);
        }
        case AST_IF: {
            Value cond = eval(node->data.if_stmt.condition, env);
            if (cond.type == VAL_RETURN || cond.type == VAL_BREAK || cond.type == VAL_CONTINUE) return cond;
            if (is_truthy(cond)) {
                return eval(node->data.if_stmt.then_branch, env);
            } else if (node->data.if_stmt.else_branch) {
                return eval(node->data.if_stmt.else_branch, env);
            }
            return val_nil();
        }
        case AST_WHILE: {
            while (1) {
                Value cond = eval(node->data.while_stmt.condition, env);
                if (cond.type == VAL_RETURN || cond.type == VAL_BREAK || cond.type == VAL_CONTINUE) return cond;
                if (!is_truthy(cond)) break;
                
                Value res = eval(node->data.while_stmt.body, env);
                if (res.type == VAL_RETURN) return res;
                if (res.type == VAL_BREAK) break;
                if (res.type == VAL_CONTINUE) continue;
            }
            return val_nil();
        }
        case AST_BREAK: return (Value){.type = VAL_BREAK};
        case AST_CONTINUE: return (Value){.type = VAL_CONTINUE};
        case AST_PASS: return val_nil();
        case AST_ERROR: {
            Value msg = eval(node->data.error_expr.message, env);
            if (msg.type == VAL_RETURN) msg = *msg.data.return_val;
            return val_error(msg.type == VAL_STR ? msg.data.s : "error");
        }
        case AST_FOR: {
            Value iter = eval(node->data.for_stmt.iterable, env);
            if (iter.type == VAL_RETURN) iter = *iter.data.return_val;
            if (iter.type != VAL_ARR) report_runtime_error(node, env, "TYPE", "Cannot iterate over non-array");
            char* var_name = node->data.for_stmt.alias ? node->data.for_stmt.alias : node->data.for_stmt.var;
            for (int i=0; i<iter.data.arr->count; i++) {
                env_define(env, var_name, *iter.data.arr->elements[i]);
                Value res = eval(node->data.for_stmt.body, env);
                if (res.type == VAL_RETURN) return res;
                if (res.type == VAL_BREAK) break;
                if (res.type == VAL_CONTINUE) continue;
            }
            return val_nil();
        }
        case AST_OBJECT: {
            Value obj = val_obj();
            AstField* f = node->data.object.fields;
            while (f) {
                if (obj.data.obj->count >= obj.data.obj->capacity) {
                    obj.data.obj->capacity *= 2;
                    obj.data.obj->keys = realloc(obj.data.obj->keys, sizeof(char*) * obj.data.obj->capacity);
                    obj.data.obj->values = realloc(obj.data.obj->values, sizeof(Value*) * obj.data.obj->capacity);
                }
                obj.data.obj->keys[obj.data.obj->count] = strdup(f->name);
                obj.data.obj->values[obj.data.obj->count] = malloc(sizeof(Value));
                Value val = eval(f->value, env);
                if (val.type == VAL_RETURN) val = *val.data.return_val;
                *obj.data.obj->values[obj.data.obj->count] = val;
                obj.data.obj->count++;
                f = f->next;
            }
            return obj;
        }
        case AST_ARRAY: {
            Value arr = val_arr();
            for (int i=0; i<node->data.array.count; i++) {
                if (arr.data.arr->count >= arr.data.arr->capacity) {
                    arr.data.arr->capacity *= 2;
                    arr.data.arr->elements = realloc(arr.data.arr->elements, sizeof(Value*) * arr.data.arr->capacity);
                }
                arr.data.arr->elements[arr.data.arr->count] = malloc(sizeof(Value));
                Value val = eval(node->data.array.elements[i], env);
                if (val.type == VAL_RETURN) val = *val.data.return_val;
                *arr.data.arr->elements[arr.data.arr->count] = val;
                arr.data.arr->count++;
            }
            return arr;
        }
        case AST_INDEX: {
            Value obj = eval(node->data.index.object, env);
            Value idx = eval(node->data.index.index, env);
            if (obj.type == VAL_ARR) {
                if (idx.type != VAL_INT) report_runtime_error(node, env, "TYPE", "Array index must be integer");
                if (idx.data.i < 0 || idx.data.i >= obj.data.arr->count) report_runtime_error(node, env, "BOUNDS", "Array index out of bounds");
                return *obj.data.arr->elements[idx.data.i];
            }
            return val_nil();
        }
        case AST_DESTRUCTURING: {
            Value val = eval(node->data.destruct.value, env);
            if (val.type == VAL_RETURN) val = *val.data.return_val;
            if (val.type != VAL_OBJ) report_runtime_error(node, env, "TYPE", "Destructuring target must be an object");
            AstField* f = node->data.destruct.target->data.object.fields;
            while (f) {
                Value field_val = val_nil();
                for (int i=0; i<val.data.obj->count; i++) {
                    if (strcmp(val.data.obj->keys[i], f->name) == 0) {
                        field_val = *val.data.obj->values[i];
                        break;
                    }
                }
                char* target_name = f->alias ? f->alias : f->name;
                env_define(env, target_name, field_val);
                f = f->next;
            }
            return val;
        }
        case AST_INDEX_ASSIGN: {
            Value obj = eval(node->data.index_assign.object, env);
            Value idx = eval(node->data.index_assign.index, env);
            Value val = eval(node->data.index_assign.value, env);
            if (obj.type == VAL_ARR) {
                if (idx.type != VAL_INT) report_runtime_error(node, env, "TYPE", "Array index must be integer");
                if (idx.data.i < 0 || idx.data.i >= obj.data.arr->count) report_runtime_error(node, env, "BOUNDS", "Array index out of bounds");
                *obj.data.arr->elements[idx.data.i] = val;
            }
            return val;
        }
        case AST_IMPORT: {
            char* clean_path = node->data.import_stmt.path;
            char full_path[512];
            char* source = NULL;

            // Try include paths
            for (int i=0; i<include_path_count; i++) {
                snprintf(full_path, sizeof(full_path), "%s/%s.nr", include_paths[i], clean_path);
                source = read_file_internal(full_path);
                if (source) break;
            }

            if (!source) {
                snprintf(full_path, sizeof(full_path), "%s.nr", clean_path);
                source = read_file_internal(full_path);
            }
            if (!source) {
                snprintf(full_path, sizeof(full_path), "lib/%s.nr", clean_path);
                source = read_file_internal(full_path);
            }
            if (!source) {
                char buf[512];
                snprintf(buf, sizeof(buf), "Could not find module '%s'", clean_path);
                report_runtime_error(node, env, "IMPORT", buf);
            }
            Lexer lex;
            lexer_init(&lex, source);
            Parser p;
            parser_init(&p, &lex);
            AstNode* imported_program = parse_program(&p);
            Environment* root = env;
            while (root->parent) root = root->parent;
            Environment* import_env = env_new(root);
            import_env->source = source;
            import_env->filename = strdup(full_path);
            eval(imported_program, import_env);
            char* final_name = node->data.import_stmt.alias;
            if (!final_name && node->data.import_stmt.symbol_count == 0) {
                char* slash = strrchr(clean_path, '/');
                final_name = slash ? slash + 1 : clean_path;
            }
            if (final_name) {
                Value mod_obj = val_obj();
                Variable* v = import_env->vars;
                while (v) {
                    if (mod_obj.data.obj->count >= mod_obj.data.obj->capacity) {
                        mod_obj.data.obj->capacity *= 2;
                        mod_obj.data.obj->keys = realloc(mod_obj.data.obj->keys, sizeof(char*) * mod_obj.data.obj->capacity);
                        mod_obj.data.obj->values = realloc(mod_obj.data.obj->values, sizeof(Value*) * mod_obj.data.obj->capacity);
                    }
                    mod_obj.data.obj->keys[mod_obj.data.obj->count] = strdup(v->name);
                    mod_obj.data.obj->values[mod_obj.data.obj->count] = malloc(sizeof(Value));
                    *mod_obj.data.obj->values[mod_obj.data.obj->count] = v->value;
                    mod_obj.data.obj->count++;
                    v = v->next;
                }
                env_define(env, final_name, mod_obj);
            } else if (node->data.import_stmt.symbol_count > 0) {
                for (int i=0; i<node->data.import_stmt.symbol_count; i++) {
                    Value v = env_get(import_env, node->data.import_stmt.symbols[i]);
                    env_define(env, node->data.import_stmt.symbols[i], v);
                }
            } else {
                Variable* v = import_env->vars;
                while (v) {
                    env_define(env, v->name, v->value);
                    v = v->next;
                }
            }
            return val_nil();
        }
        case AST_EXPORT: {
            return eval(node->data.func_decl.body, env); 
        }
        default:
            return val_nil();
    }
}
