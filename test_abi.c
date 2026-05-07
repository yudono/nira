#include <stdio.h>

typedef struct {
    int type;
    void* data;
} Value;

Value my_func(Value a) {
    printf("a.type = %d\n", a.type);
    return a;
}

int main() {
    Value (*f)(Value, Value, Value) = (Value (*)(Value, Value, Value))my_func;
    Value a = {1, NULL};
    Value b = {2, NULL};
    Value c = {3, NULL};
    f(a, b, c);
    return 0;
}
