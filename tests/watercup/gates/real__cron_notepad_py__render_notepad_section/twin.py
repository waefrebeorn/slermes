"""twin for cron/notepad.py:render_notepad_section (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/notepad.py"
    PY_FN = "render_notepad_section"
    C_FN = "render_notepad_section"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
