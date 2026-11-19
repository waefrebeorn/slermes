"""twin for tools/bot_mode_probe.py:is_bot_mode_managed (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_probe.py"
    PY_FN = "is_bot_mode_managed"
    C_FN = "bot_mode_probe_is_bot_mode_managed"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
