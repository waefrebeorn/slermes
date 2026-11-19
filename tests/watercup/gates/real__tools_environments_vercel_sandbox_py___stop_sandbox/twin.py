"""twin for tools/environments/vercel_sandbox.py:_stop_sandbox (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/vercel_sandbox.py"
    PY_FN = "_stop_sandbox"
    C_FN = "ve_env_stop_sandbox"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
