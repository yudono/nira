export main:
  print("--- Testing Math ---")
  import math
  res = math.sum([10, 20])
  print(res) # 30

  r = math.random()
  print("Random: " + r)

  print("--- Testing Array ---")
  import array
  nums = [1, 2, 3, 4, 5]
  
  doubled = array.map(nums, i -> i * 2)
  print(doubled) # [2, 4, 6, 8, 10]

  total = array.reduce(nums, 0, (acc, i) -> acc + i)
  print(total) # 15

  print("--- Testing String ---")
  import string
  s = "Hello World"
  print(string.substring(s, 0, 5)) # Hello
  print(string.concat(s, "!!!")) # Hello World!!!
  print(string.length(s)) # 11

  print("--- Testing File ---")
  import file
  file.write("test.txt", "Nira rules!")
  content = file.read("test.txt")
  print("File content: " + content)
  file.remove("test.txt")

  print("--- Testing Time & Sys ---")
  from time import now
  import sys
  print("Current time: " + now())
  print("Sleeping 100ms...")
  import sleep from time
  sleep(100)
  print("Done.")
