"""twin for hermes_cli/main.py:_filter_pending_shim_renames (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/main.py"
    PY_FN = "_filter_pending_shim_renames"
    C_FN = "_filter_pending_shim_renames"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
