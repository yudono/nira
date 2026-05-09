#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "ast.h"

typedef enum {
    VAL_NIL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_OBJ,
    VAL_ARR,
    VAL_FUNC,
    VAL_RETURN,
    VAL_BOOL,
    VAL_BREAK,
    VAL_CONTINUE,
    VAL_ERROR
} ValueType;

typedef struct Object {
    char** keys;
    struct Value** values;
    int count;
    int capacity;
} Object;

typedef struct Array {
    struct Value** elements;
    int count;
    int capacity;
} Array;

typedef struct Value {
    ValueType type;
    int length;
    union {
        long long i;
        double f;
        char* s;
        Object* obj;
        Array* arr;
        struct {
            AstNode* decl;
            struct Environment* closure;
        } func;
        struct Value* return_val;
    } data;
} Value;

typedef struct Variable {
    char* name;
    Value value;
    struct Variable* next;
} Variable;

typedef struct Environment {
    struct Environment* parent;
    Variable** table;
    Variable* fast_table[4]; // [NEW] Small fixed-size array for fast access
    int capacity;
    int count;
    Value* slots;     // [NEW] Fixed-size array for parameters/locals
    int slot_count;   // [NEW]
    int slots_capacity; // [NEW] Track capacity for reuse
    const char* source;
    const char* filename;
} Environment;

Value val_int(long long i);
Value val_float(double f);
Value val_str(char* s);
Value val_str_len(char* s, int len);
Value val_obj();
Value val_arr();
Value val_func(AstNode* decl, Environment* closure);
Value val_nil();
Value val_return(Value v);
Value val_bool(int b);
Value val_error(char* msg);
void val_free(Value v);

Environment* env_new(Environment* parent, int slot_count);
void env_define(Environment* env, char* name, Value val);
void env_assign(Environment* env, char* name, Value val);
Value env_get(Environment* env, char* name);
void env_free(Environment* env);

void nr_resolve(AstNode* program);
Value eval(AstNode* node, Environment* env);

// FFI Compatibility Functions
void* nr_alloc(size_t sz);
char* nr_strdup(const char* s);
Value nr_rt_push(Value arr, Value val);
void set_field(Value obj, const char* key, Value val);
Value get_field(Value obj, const char* key);
void print_value(Value v);

#endif
