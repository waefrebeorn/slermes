"""twin for hermes_cli/update_receipt.py:_prune_old_receipts (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_receipt.py"
    PY_FN = "_prune_old_receipts"
    C_FN = "prune_old_receipts"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
