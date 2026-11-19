"""twin for agent/curator.py:_build_rename_summary (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/curator.py"
    PY_FN = "_build_rename_summary"
    C_FN = "_build_rename_summary"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
