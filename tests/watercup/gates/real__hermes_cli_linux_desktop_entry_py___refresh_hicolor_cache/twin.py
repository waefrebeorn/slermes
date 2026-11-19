"""twin for hermes_cli/linux_desktop_entry.py:_refresh_hicolor_cache (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/linux_desktop_entry.py"
    PY_FN = "_refresh_hicolor_cache"
    C_FN = "_refresh_hicolor_cache"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
