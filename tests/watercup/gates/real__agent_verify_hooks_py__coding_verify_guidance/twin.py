"""twin for agent/verify_hooks.py:coding_verify_guidance (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify_hooks.py"
    PY_FN = "coding_verify_guidance"
    C_FN = "coding_verify_guidance"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
