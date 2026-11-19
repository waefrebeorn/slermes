"""twin for hermes_cli/heartbeat.py:_INTERVAL_RE (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/heartbeat.py"
    PY_FN = "_INTERVAL_RE"
    C_FN = "_INTERVAL_RE"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
