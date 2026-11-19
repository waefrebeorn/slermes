"""twin for agent/context_references.py:_iter_visible_entries (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_references.py"
    PY_FN = "_iter_visible_entries"
    C_FN = "_iter_visible_entries"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
