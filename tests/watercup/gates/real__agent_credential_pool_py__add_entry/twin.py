"""twin for agent/credential_pool.py:add_entry (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/credential_pool.py"
    PY_FN = "add_entry"
    C_FN = "add_entry"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
