"""twin for hermes_cli/input_sanitize.py:strip_leaked_bracketed_paste_wrappers (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/input_sanitize.py"
    PY_FN = "strip_leaked_bracketed_paste_wrappers"
    C_FN = "strip_leaked_bracketed_paste_wrappers"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
