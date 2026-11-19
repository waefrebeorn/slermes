"""twin for tools/environments/vercel_sandbox.py:_wait_for_running (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/vercel_sandbox.py"
    PY_FN = "_wait_for_running"
    C_FN = "_wait_for_running"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
