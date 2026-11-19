"""twin for agent/credential_pool.py:mark_exhausted_and_rotate (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "mark_exhausted_and_rotate"
    C_FN = "mark_exhausted_and_rotate"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
