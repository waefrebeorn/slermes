"""twin for hermes_cli/main.py:_desktop_macos_setup_tcc_identity (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_desktop_macos_setup_tcc_identity"
    C_FN = "_desktop_macos_setup_tcc_identity"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
