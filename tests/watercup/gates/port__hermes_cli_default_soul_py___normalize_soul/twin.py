"""twin for hermes_cli/default_soul.py:_normalize_soul (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/default_soul.py"
    PY_FN = "_normalize_soul"
    C_FN = "hermes_cli_default_soul_normalize"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
