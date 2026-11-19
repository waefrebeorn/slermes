"""twin for hermes_cli/status.py:_format_relative_ts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/status.py"
    PY_FN = "_format_relative_ts"
    C_FN = "_format_relative_ts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
