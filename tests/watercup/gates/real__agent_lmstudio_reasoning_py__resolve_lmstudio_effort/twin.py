"""twin for agent/lmstudio_reasoning.py:resolve_lmstudio_effort (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/lmstudio_reasoning.py"
    PY_FN = "resolve_lmstudio_effort"
    C_FN = "resolve_lmstudio_effort"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
