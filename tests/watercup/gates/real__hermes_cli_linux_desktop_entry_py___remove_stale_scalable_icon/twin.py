"""twin for hermes_cli/linux_desktop_entry.py:_remove_stale_scalable_icon (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/linux_desktop_entry.py"
    PY_FN = "_remove_stale_scalable_icon"
    C_FN = "_remove_stale_scalable_icon"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
