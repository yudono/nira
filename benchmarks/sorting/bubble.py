import time

def main():
    arr = list(range(1000, 0, -1))
    
    start = time.time() * 1000
    n = len(arr)
    for i in range(n):
        for j in range(0, n - i - 1):
            if arr[j] > arr[j+1]:
                arr[j], arr[j+1] = arr[j+1], arr[j]
    end = time.time() * 1000
    
    print(f"Python: {int(end - start)} ms (First: {arr[0]}, Last: {arr[-1]})")

main()
