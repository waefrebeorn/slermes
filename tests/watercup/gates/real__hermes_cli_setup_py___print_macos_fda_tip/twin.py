"""twin for hermes_cli/setup.py:_print_macos_fda_tip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/setup.py"
    PY_FN = "_print_macos_fda_tip"
    C_FN = "_print_macos_fda_tip"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
