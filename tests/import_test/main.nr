import "config/config"
import time
import math
from math import sqrt

main:
  a = 10
  # a = "20"
  b = 30
  c = a + b
  println("c: " + toString(c))
  
  # Test workspace import
  println("Config DB: " + config.db)
  
  # Test library import
  now = time.now()
  println("Current time: " + toString(now))
  
  # Test from import
  res = sqrt(16)
  println("Sqrt(16): " + toString(res))
  
  # Test math library object
  println("Random: " + toString(math.random()))
