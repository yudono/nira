import "api"
import "cli"
import "sys"

args = sys.args()

main:
  if len(args) > 1:
    cli.run(args)
  else:
    api.main()
