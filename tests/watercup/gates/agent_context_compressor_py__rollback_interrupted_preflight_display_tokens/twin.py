"""twin for agent/context_compressor.py:rollback_interrupted_preflight_display_tokens (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "rollback_interrupted_preflight_display_tokens"
    C_FN = "ctxc_rollback_interrupted_preflight_display_tokens"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
