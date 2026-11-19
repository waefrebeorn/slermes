"""twin for tools/environments/daytona.py:cleanup (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/daytona.py"
    PY_FN = "cleanup"
    C_FN = "daytona_delete_sandbox"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
