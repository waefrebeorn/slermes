"""twin for tools/file_state.py:_fmt_ts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/file_state.py"
    PY_FN = "_fmt_ts"
    C_FN = "_fmt_ts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
