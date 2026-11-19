"""twin for hermes_cli/clipboard.py:_powershell_has_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/clipboard.py"
    PY_FN = "_powershell_has_image"
    C_FN = "clipboard_powershell_has_image"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
