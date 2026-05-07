run args:
  for t in items:
    status = "[ ]"
    if t.done:
      status = "[x]"
    print(status + " " + toString(t.id) + ": " + t.title)
