"""twin for hermes_cli/clipboard.py:save_clipboard_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/clipboard.py"
    PY_FN = "save_clipboard_image"
    C_FN = "save_clipboard_image"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
