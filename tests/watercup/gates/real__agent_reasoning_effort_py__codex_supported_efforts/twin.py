"""twin for agent/reasoning_effort.py:codex_supported_efforts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/reasoning_effort.py"
    PY_FN = "codex_supported_efforts"
    C_FN = "codex_supported_efforts"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
