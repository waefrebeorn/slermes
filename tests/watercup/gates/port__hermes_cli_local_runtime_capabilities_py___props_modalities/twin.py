"""twin for hermes_cli/local_runtime/capabilities.py:_props_modalities (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/local_runtime/capabilities.py"
    PY_FN = "_props_modalities"
    C_FN = "_props_modalities"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
