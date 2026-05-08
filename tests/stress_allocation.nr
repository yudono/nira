# Massive Object Allocation Stress Test
# Tests memory management and Arena allocator.

create_small_object id:
  return {
    id: id,
    type: "test",
    value: id * 1.5,
    flag: true,
    data: "some random string data to fill memory"
  }

main:
  print("=== MASSIVE ALLOCATION TEST ===")
  
  iterations = 50
  batch_size = 1000
  
  i = 0
  while i < iterations:
    print("Batch " + (i + 1) + " / " + iterations)
    batch = []
    j = 0
    while j < batch_size:
      batch.push(create_small_object(i * batch_size + j))
      j = j + 1
    
    # Check first and last in batch
    first = batch[0]
    last = batch[batch_size - 1]
    # print("  First ID: " + first.id + ", Last ID: " + last.id)
    
    # batch goes out of scope here, ideally memory is reclaimed or handled by Arena
    i = i + 1
  
  print("=== ALLOCATION TEST PASSED ===")
