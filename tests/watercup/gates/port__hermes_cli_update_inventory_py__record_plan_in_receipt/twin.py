"""twin for hermes_cli/update_inventory.py:record_plan_in_receipt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_inventory.py"
    PY_FN = "record_plan_in_receipt"
    C_FN = "record_plan_in_receipt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
