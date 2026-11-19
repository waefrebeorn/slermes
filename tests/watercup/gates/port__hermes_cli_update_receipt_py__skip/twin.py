"""twin for hermes_cli/update_receipt.py:skip (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_receipt.py"
    PY_FN = "skip"
    C_FN = "update_receipt_skip"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
