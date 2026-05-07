from time import millis

main:
    start = millis()
    s = ""
    i = 0
    while i < 10000:
        s = s + "a"
        i = i + 1
    end = millis()
    
    print("Nira: " + toString(end - start) + " ms (Length: " + toString(len(s)) + ")")
