# Time Library for Nira

export now:
  return __builtin_time_now()

export sleep ms:
  return __builtin_delay(ms)

export millis:
  return __builtin_millis()
