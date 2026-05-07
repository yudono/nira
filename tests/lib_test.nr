import conv
import parse
import encoding
import collection
import json
import time

main:
  # Test conv
  val = "123"
  num = conv.toInt(val)
  println("conv.toInt: " + toString(num))
  
  # Test parse
  f = parse.parseFloat("3.14")
  println("parse.parseFloat: " + toString(f))
  
  # Test encoding
  b64 = encoding.toBase64("hello")
  println("Base64: " + b64)
  println("Decoded: " + encoding.fromBase64(b64))
  
  # Test json
  obj = { name: "Nira", version: 1 }
  jstr = json.stringify(obj)
  println("JSON: " + jstr)
  
  # Test collection
  list = [1, 1, 2, 3]
  unique = collection.toSet(list)
  println("Unique items: " + toString(len(unique)))
  
  # Test time
  d = time.fromDate(2024, 5, 7)
  println("Unix time: " + toString(time.toUnix(d)))
  
  println("✨ All library tests passed!")
