"""twin for hermes_cli/clipboard.py:_write_base64_image (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/clipboard.py"
    PY_FN = "_write_base64_image"
    C_FN = "clipboard_write_base64_image"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
