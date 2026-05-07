import time
arr = [5000 - i for i in range(5000)]
start = time.time() * 1000
n = len(arr)
for i in range(n):
    for j in range(n - i - 1):
        if arr[j] > arr[j + 1]:
            arr[j], arr[j + 1] = arr[j + 1], arr[j]
end = time.time() * 1000
print(f"Python: {int(end - start)} ms (First: {arr[0]}, Last: {arr[4999]})")
