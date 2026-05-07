# ============================================
# Master Stress Test Runner
# ============================================
# Runs all comprehensive stress tests from TESTING.md
#
# Categories:
#   1. High-Precision Arithmetic
#   2. Memory & Recursion
#   3. Data Structures
#   4. Edge Cases & Error Handling
#   5. I/O Stability
#   6. Closures, Scope & Composition

import "stress_arithmetic"
import "stress_recursion"
import "stress_datastructures"
import "stress_edge_cases"
import "stress_io"
import "stress_closures"

main:
  print("╔══════════════════════════════════════╗")
  print("║   NIRA COMPREHENSIVE STRESS SUITE    ║")
  print("╚══════════════════════════════════════╝")
  print("")

  stress_arithmetic.main()
  print("")
  print("────────────────────────────────────────")
  print("")

  stress_recursion.main()
  print("")
  print("────────────────────────────────────────")
  print("")

  stress_datastructures.main()
  print("")
  print("────────────────────────────────────────")
  print("")

  stress_edge_cases.main()
  print("")
  print("────────────────────────────────────────")
  print("")

  stress_io.main()
  print("")
  print("────────────────────────────────────────")
  print("")

  stress_closures.main()
  print("")
  print("╔══════════════════════════════════════╗")
  print("║   ALL STRESS TESTS COMPLETED ✅      ║")
  print("╚══════════════════════════════════════╝")
