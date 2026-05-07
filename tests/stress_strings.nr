# ============================================
# Test: String Features (Escape Sequences & Multi-line)
# ============================================
# Tests \n, \t, \\, \", multi-line strings ("""),
# and string operations with special characters.

export main:
  print("=== STRESS: String Features ===")

  # --- Escape sequences ---
  print("Escape Sequences:")
  hello = "Hello\nWorld"
  print(hello)

  tabbed = "Col1\tCol2\tCol3"
  print(tabbed)

  backslash = "path\\to\\file"
  print(backslash)

  quoted = "She said \"hello\""
  print(quoted)

  mixed = "Line1\nLine2\n\tIndented"
  print(mixed)

  # --- Multi-line strings ---
  print("")
  print("Multi-line Strings:")
  multi = """This is
a multi-line
string"""
  print(multi)

  multi_with_code = """function main:
  x = 10
  print(x)"""
  print(multi_with_code)

  # --- Escape in concatenation ---
  print("")
  print("Escape + Concat:")
  prefix = "Name:\t"
  name = "Budi"
  print(prefix + name)

  lines = "A\nB\nC"
  print("Lines: " + lines)

  # --- Empty and edge strings ---
  print("")
  print("Edge Cases:")
  empty = ""
  print("empty len: " + len(empty))

  single = "x"
  print("single: " + single)

  # --- String with numbers ---
  print("")
  print("String + Number Coercion:")
  result = "Score: " + 42 + "\nGrade: A"
  print(result)

  # --- Long string with escapes ---
  print("")
  print("Long Escaped String:")
  long_str = "Row1\tA\tB\tC\nRow2\tD\tE\tF\nRow3\tG\tH\tI"
  print(long_str)

  print("")
  print("=== String Features PASSED ===")
