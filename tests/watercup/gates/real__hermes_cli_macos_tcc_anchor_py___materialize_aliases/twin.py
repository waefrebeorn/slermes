"""twin for hermes_cli/macos_tcc_anchor.py:_materialize_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/macos_tcc_anchor.py"
    PY_FN = "_materialize_aliases"
    C_FN = "_materialize_aliases"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
