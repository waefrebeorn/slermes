"""twin for agent/turn_summary.py:format_elapsed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/turn_summary.py"
    PY_FN = "format_elapsed"
    C_FN = "format_elapsed"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
