from time import millis

main:
    # Initialize array with 1000 numbers in reverse
    arr = []
    i = 0
    while i < 1000:
        arr.push(1000 - i)
        i = i + 1
    
    start = millis()
    
    # Bubble Sort
    n = len(arr)
    i = 0
    while i < n:
        j = 0
        while j < n - i - 1:
            if arr[j] > arr[j+1]:
                temp = arr[j]
                arr[j] = arr[j+1]
                arr[j+1] = temp
            j = j + 1
        i = i + 1
        
    end = millis()
    
    print("Nira: " + toString(end - start) + " ms (First: " + toString(arr[0]) + ", Last: " + toString(arr[999]) + ")")
