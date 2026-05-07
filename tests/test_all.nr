# running all test
import "alias_test"
import "basic_test"
import "control_flow"
import "lib_test"
import "system_test"


main:
  alias_test.main()
  basic_test.main()
  control_flow.main()
  lib_test.main()
  system_test.main()