"""twin for hermes_cli/macos_tcc_anchor.py:tcc_anchor_state (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "tcc_anchor_state"
    C_FN = "tcc_anchor_state"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
