"""twin for hermes_cli/macos_tcc_anchor.py:_write_marker (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "_write_marker"
    C_FN = "_write_marker"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
