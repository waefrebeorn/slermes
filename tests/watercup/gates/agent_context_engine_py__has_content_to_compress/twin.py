"""twin for agent/context_engine.py:has_content_to_compress (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_engine.py"
    PY_FN = "has_content_to_compress"
    C_FN = "ceg_has_content_to_compress"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
