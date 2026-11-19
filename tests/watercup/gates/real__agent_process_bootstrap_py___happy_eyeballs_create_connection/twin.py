"""twin for agent/process_bootstrap.py:_happy_eyeballs_create_connection (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"
    PY_FN = "_happy_eyeballs_create_connection"
    C_FN = "_happy_eyeballs_create_connection"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
