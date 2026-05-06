#ifndef EVALUATOR_H
#define EVALUATOR_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_STR,
    VAL_OBJ,
    VAL_ARR,
    VAL_FUNC,
    VAL_NIL,
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
    union {
        int i;
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
    Variable* vars;
    struct Environment* parent;
    const char* source;
    const char* filename;
} Environment;

Value val_int(int i);
Value val_str(char* s);
Value val_obj();
Value val_arr();
Value val_func(AstNode* decl, Environment* closure);
Value val_nil();
Value val_return(Value v);
Value val_bool(int b);
Value val_error(char* msg);
void val_free(Value v);

Environment* env_new(Environment* parent);
void env_define(Environment* env, char* name, Value val);
void env_assign(Environment* env, char* name, Value val);
Value env_get(Environment* env, char* name);
void env_free(Environment* env);

Value eval(AstNode* node, Environment* env);

#endif
