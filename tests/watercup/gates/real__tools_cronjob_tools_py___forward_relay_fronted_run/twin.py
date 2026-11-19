"""twin for tools/cronjob_tools.py:_forward_relay_fronted_run (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/cronjob_tools.py"
    PY_FN = "_forward_relay_fronted_run"
    C_FN = "_forward_relay_fronted_run"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
