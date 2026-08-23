import io, json, sys
from tools.computer_use.doctor import _print_text_report
rep = json.load(sys.stdin)
buf = io.StringIO()
_print_text_report(rep, True, identity={"resolved_binary":"/usr/bin/cua-driver","cli_version":"1.0","version_mismatch":True})
open('/tmp/p_out.txt','w').write(buf.getvalue())
