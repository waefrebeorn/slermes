"""twin for hermes_cli/inventory.py:_apply_custom_aliases (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/inventory.py"
    PY_FN = "_apply_custom_aliases"
    C_FN = "_apply_custom_aliases"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
