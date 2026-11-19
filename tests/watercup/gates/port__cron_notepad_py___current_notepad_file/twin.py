"""twin for cron/notepad.py:_current_notepad_file (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/notepad.py"
    PY_FN = "_current_notepad_file"
    C_FN = "_current_notepad_file"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
