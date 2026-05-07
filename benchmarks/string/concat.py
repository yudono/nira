import time
start = time.time() * 1000
s = ""
for i in range(100000):
    s += "a"
end = time.time() * 1000
print(f"Python: {int(end - start)} ms (Length: {len(s)})")
