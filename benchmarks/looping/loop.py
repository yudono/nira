import time
start = time.time() * 1000
sum = 0
for i in range(100000000):
    sum += i
end = time.time() * 1000
print(f"Python: {int(end - start)} ms (Result: {sum})")
