# Data Representation
# Konversi data ke format yang bisa dikirim melalui jaringan atau disimpan.

export toBase64(data):
  return __builtin_encoding_to_base64(data)

export fromBase64(str):
  return __builtin_encoding_from_base64(str)

export toHex(data):
  return __builtin_encoding_to_hex(data)

# Byte conversion (Placeholders for future implementation)
export toBytes(str):
  return str

export fromBytes(bytes):
  return bytes
