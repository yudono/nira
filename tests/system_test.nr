# Test System commands
from file import write, read, remove
from sys import run

greet(data):
  print("Hello " + data.name)
  print("Age " + data.age)
  return 0

export main():
  greet({ name: "Budi", age: "25" })
  
  # Write to file
  write("test.txt", "Hello from Nira!")
  
  # Read from file
  content = read("test.txt")
  print(content)
  
  # Exec command
  run("ls -l test.txt")
  remove("test.txt")
