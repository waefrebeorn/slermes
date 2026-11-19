"""twin for hermes_cli/pets.py:_print (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/pets.py"
    PY_FN = "_print"
    C_FN = "skill_bundles_print"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
