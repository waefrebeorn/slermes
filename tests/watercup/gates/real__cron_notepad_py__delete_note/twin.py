"""twin for cron/notepad.py:delete_note (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cron/notepad.py"
    PY_FN = "delete_note"
    C_FN = "cron_notepad_delete_note"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
