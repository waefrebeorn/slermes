"""twin for hermes_cli/macos_tcc_anchor.py:_install_anchor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "_install_anchor"
    C_FN = "_install_anchor"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
