"""twin for hermes_cli/macos_tcc_anchor.py:ensure_tcc_anchor (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "ensure_tcc_anchor"
    C_FN = "ensure_tcc_anchor"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
