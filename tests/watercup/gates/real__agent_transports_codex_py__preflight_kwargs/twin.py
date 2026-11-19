"""twin for agent/transports/codex.py:preflight_kwargs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "preflight_kwargs"
    C_FN = "preflight_kwargs"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
