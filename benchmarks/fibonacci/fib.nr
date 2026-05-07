from time import millis

fib n:
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)

main:
    start = millis()
    result = fib(40)
    end = millis()
    print("Nira: " + toString(end - start) + " ms (Result: " + toString(result) + ")")
