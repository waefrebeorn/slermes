"""twin for cli.py:_should_auto_attach_clipboard_image_on_paste (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "cli.py"
    PY_FN = "_should_auto_attach_clipboard_image_on_paste"
    C_FN = "_should_auto_attach_clipboard_image_on_paste"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
