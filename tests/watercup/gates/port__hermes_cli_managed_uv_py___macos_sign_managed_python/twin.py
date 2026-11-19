"""twin for hermes_cli/managed_uv.py:_macos_sign_managed_python (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/managed_uv.py"
    PY_FN = "_macos_sign_managed_python"
    C_FN = "_macos_sign_managed_python"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
