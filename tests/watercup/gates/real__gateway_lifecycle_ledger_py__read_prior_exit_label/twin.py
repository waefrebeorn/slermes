"""twin for gateway/lifecycle_ledger.py:read_prior_exit_label (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/lifecycle_ledger.py"
    PY_FN = "read_prior_exit_label"
    C_FN = "read_prior_exit_label"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
