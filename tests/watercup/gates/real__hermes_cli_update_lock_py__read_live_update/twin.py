"""twin for hermes_cli/update_lock.py:read_live_update (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_lock.py"
    PY_FN = "read_live_update"
    C_FN = "read_live_update"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
