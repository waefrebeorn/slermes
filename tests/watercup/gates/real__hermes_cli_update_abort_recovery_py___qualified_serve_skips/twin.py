"""twin for hermes_cli/update_abort_recovery.py:_qualified_serve_skips (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/update_abort_recovery.py"
    PY_FN = "_qualified_serve_skips"
    C_FN = "_qualified_serve_skips"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
