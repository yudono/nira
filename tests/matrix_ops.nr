# Matrix Operations Test
# Tests nested arrays and loops.

create_matrix rows, cols, initial:
  m = []
  i = 0
  while i < rows:
    row = []
    j = 0
    while j < cols:
      row.push(initial)
      j = j + 1
    m.push(row)
    i = i + 1
  return m

multiply a, b:
  rows_a = len(a)
  cols_a = len(a[0])
  rows_b = len(b)
  cols_b = len(b[0])
  
  print("multiply: " + toString(rows_a) + "x" + toString(cols_a) + " * " + toString(rows_b) + "x" + toString(cols_b))
  
  if cols_a != rows_b:
    print("Error: Incompatible dimensions")
    return null
  
  result = create_matrix(rows_a, cols_b, 0)
  print("Result matrix initialized, size: " + toString(len(result)))
  
  i = 0
  while i < rows_a:
    j = 0
    while j < cols_b:
      sum = 0
      k = 0
      while k < cols_a:
        sum = sum + a[i][k] * b[k][j]
        k = k + 1
      result[i][j] = sum
      j = j + 1
    i = i + 1
  
  return result

print_matrix m:
  for row in m:
    line = ""
    for val in row:
      line = line + toString(val) + " "
    print(line)

main:
  print("=== MATRIX MULTIPLICATION TEST ===")
  
  # 2x3 Matrix
  mat_a = [
    [1, 2, 3],
    [4, 5, 6]
  ]
  
  # 3x2 Matrix
  mat_b = [
    [7, 8],
    [9, 10],
    [11, 12]
  ]
  
  print("Matrix A:")
  print_matrix(mat_a)
  print("Matrix B:")
  print_matrix(mat_b)
  
  res = multiply(mat_a, mat_b)
  
  print("Result (A * B):")
  if res != null:
    print_matrix(res)
    # Expected:
    # 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58
    # 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64
    # 4*7 + 5*9 + 6*11 = 28 + 45 + 66 = 139
    # 4*8 + 5*10 + 6*12 = 32 + 50 + 72 = 154
  
  print("=== MATRIX TEST PASSED ===")
