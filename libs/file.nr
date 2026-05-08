# File Library for Nira

# Read file
export read(path):
  return __builtin_file_read(path)

# Write file
export write(path, content):
  return __builtin_file_write(path, content)

# Delete file
export remove(path):
  return __builtin_file_delete(path)

# Append file
export append(path, content):
  existing = __builtin_file_read(path)
  return __builtin_file_write(path, existing + content)

# Check exists
export exists(path):
  return __builtin_file_exists(path)

# Copy file
export copy(src, dest):
  data = __builtin_file_read(src)
  return __builtin_file_write(dest, data)

# Move file
export move(src, dest):
  data = __builtin_file_read(src)
  __builtin_file_write(dest, data)
  return __builtin_file_delete(src)
