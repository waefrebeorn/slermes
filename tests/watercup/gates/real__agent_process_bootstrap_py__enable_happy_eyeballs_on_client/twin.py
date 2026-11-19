"""twin for agent/process_bootstrap.py:enable_happy_eyeballs_on_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/process_bootstrap.py"
    PY_FN = "enable_happy_eyeballs_on_client"
    C_FN = "enable_happy_eyeballs_on_client"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
