import time

def main():
    start = time.time() * 1000
    total = 0
    i = 0
    while i < 10000000:
        total += i
        i += 1
    end = time.time() * 1000
    print(f"Python: {int(end - start)} ms (Result: {total})")

main()
