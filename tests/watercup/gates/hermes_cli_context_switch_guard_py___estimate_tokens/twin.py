"""twin for hermes_cli/context_switch_guard.py:_estimate_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/context_switch_guard.py"
    PY_FN = "_estimate_tokens"
    C_FN = "estimate_tokens"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
