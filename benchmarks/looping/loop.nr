from time import millis

main():
    start = millis()
    sum = 0
    i = 0
    while i < 100000000:
        sum = sum + i
        i = i + 1
    
    end = millis()
    print("Nira: " + toString(end - start) + " ms (Result: " + toString(sum) + ")")
