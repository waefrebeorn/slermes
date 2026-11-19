"""twin for hermes_cli/update_lock.py:__exit__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_lock.py"
    PY_FN = "__exit__"
    C_FN = "__exit__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
