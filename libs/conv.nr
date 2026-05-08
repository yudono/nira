# Core Type Conversions
# Modul utama untuk mengubah tipe data primitif secara eksplisit.

export toInt(any, base):
  return __builtin_conv_to_int(any, base)

export toFloat(any):
  return __builtin_conv_to_float(any)

export toStr(any):
  return __builtin_conv_to_str(any)

export toBool(any):
  return __builtin_conv_to_bool(any)
