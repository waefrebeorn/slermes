"""twin for tools/environments/vercel_sandbox.py:_close_sandbox_client (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/vercel_sandbox.py"
    PY_FN = "_close_sandbox_client"
    C_FN = "ve_env_close_sandbox_client"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
