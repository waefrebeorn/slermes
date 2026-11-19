"""twin for tools/bot_mode_probe.py:stored_prompt_capability_stale (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_probe.py"
    PY_FN = "stored_prompt_capability_stale"
    C_FN = "stored_prompt_capability_stale"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
