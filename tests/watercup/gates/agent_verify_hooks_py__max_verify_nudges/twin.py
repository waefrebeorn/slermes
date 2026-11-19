"""twin for agent/verify_hooks.py:max_verify_nudges (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/verify_hooks.py"
    PY_FN = "max_verify_nudges"
    C_FN = "agent_verify_hooks_max_verify_nudges"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
