"""twin for hermes_cli/image_provenance.py:_invalid (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/image_provenance.py"
    PY_FN = "_invalid"
    C_FN = "_invalid"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
