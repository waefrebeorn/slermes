"""twin for hermes_cli/update_receipt.py:begin_update_receipt (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_receipt.py"
    PY_FN = "begin_update_receipt"
    C_FN = "begin_update_receipt"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
