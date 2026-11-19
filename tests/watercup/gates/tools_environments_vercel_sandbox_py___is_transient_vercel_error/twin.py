"""twin for tools/environments/vercel_sandbox.py:_is_transient_vercel_error (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/environments/vercel_sandbox.py"
    PY_FN = "_is_transient_vercel_error"
    C_FN = "ve_is_transient_vercel_error"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
