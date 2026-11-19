"""twin for hermes_cli/update_inventory.py:print_update_plan (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_inventory.py"
    PY_FN = "print_update_plan"
    C_FN = "print_update_plan"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
