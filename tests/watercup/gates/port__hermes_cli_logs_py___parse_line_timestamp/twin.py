"""twin for hermes_cli/logs.py:_parse_line_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/logs.py"
    PY_FN = "_parse_line_timestamp"
    C_FN = "cli_logs_parse_line_timestamp"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
