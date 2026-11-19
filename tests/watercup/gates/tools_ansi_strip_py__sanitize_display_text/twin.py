"""twin for tools/ansi_strip.py:sanitize_display_text (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/ansi_strip.py"
    PY_FN = "sanitize_display_text"
    C_FN = "tools_ansi_strip_sanitize_display_text"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
