"""twin for agent/system_prompt.py:reconstruct_static_prefix (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/system_prompt.py"
    PY_FN = "reconstruct_static_prefix"
    C_FN = "reconstruct_static_prefix"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
