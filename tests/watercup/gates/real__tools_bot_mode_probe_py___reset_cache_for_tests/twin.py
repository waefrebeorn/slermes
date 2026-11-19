"""twin for tools/bot_mode_probe.py:_reset_cache_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/bot_mode_probe.py"
    PY_FN = "_reset_cache_for_tests"
    C_FN = "bot_mode_probe_reset_cache_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
