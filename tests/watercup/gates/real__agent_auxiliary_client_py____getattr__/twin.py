"""twin for agent/auxiliary_client.py:__getattr__ (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/auxiliary_client.py"
    PY_FN = "__getattr__"
    C_FN = "__getattr__"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
