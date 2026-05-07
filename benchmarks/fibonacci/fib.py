import time

def fib(n):
    if n < 2: return n
    return fib(n-1) + fib(n-2)

start = time.time()
result = fib(35)
end = time.time()

print(f"Python: {int((end - start) * 1000)} ms (Result: {result})")
