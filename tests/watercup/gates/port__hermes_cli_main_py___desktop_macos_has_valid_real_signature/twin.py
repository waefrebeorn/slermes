"""twin for hermes_cli/main.py:_desktop_macos_has_valid_real_signature (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_desktop_macos_has_valid_real_signature"
    C_FN = "main_g_desktop_macos_has_valid_real_signature"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
