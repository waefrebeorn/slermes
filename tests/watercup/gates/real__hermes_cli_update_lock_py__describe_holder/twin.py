"""twin for hermes_cli/update_lock.py:describe_holder (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_lock.py"
    PY_FN = "describe_holder"
    C_FN = "describe_holder"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
