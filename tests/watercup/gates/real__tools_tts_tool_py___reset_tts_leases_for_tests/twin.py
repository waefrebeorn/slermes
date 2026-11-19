"""twin for tools/tts_tool.py:_reset_tts_leases_for_tests (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/tts_tool.py"
    PY_FN = "_reset_tts_leases_for_tests"
    C_FN = "_reset_tts_leases_for_tests"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
