# sys library for Nira

args():
  return __builtin_args()

now():
  return __builtin_time_now()
  
run(cmd):
  return __builtin_sys_run(cmd)

exit(code):
  __builtin_exit_proc(code)
