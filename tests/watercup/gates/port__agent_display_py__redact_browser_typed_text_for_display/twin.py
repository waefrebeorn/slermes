"""twin for agent/display.py:redact_browser_typed_text_for_display (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/display.py"
    PY_FN = "redact_browser_typed_text_for_display"
    C_FN = "redact_browser_typed_text_for_display"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
