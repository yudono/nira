# Time Library for Nira

export now:
  return __builtin_time_now()

export sleep ms:
  return __builtin_delay(ms)

export toUnix time_obj:
  return __builtin_time_to_unix(time_obj)

export fromDate year, month, day:
  return {
    year: year
    month: month
    day: day
  }

export toString time_obj, layout:
  # Basic string representation
  return __builtin_conv_to_str(time_obj)
