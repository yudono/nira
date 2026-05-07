import time

def fib(n):
    if n < 2: return n
    return fib(n - 1) + fib(n - 2)

start = time.time() * 1000
result = fib(40)
end = time.time() * 1000
print(f"Python: {int(end - start)} ms (Result: {result})")
