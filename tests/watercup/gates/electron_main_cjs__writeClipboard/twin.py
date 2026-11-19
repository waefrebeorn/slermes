"""twin for electron/main.cjs:writeClipboard (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "electron/main.cjs"
    PY_FN = "writeClipboard"
    C_FN = "clipboard_write_text"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
