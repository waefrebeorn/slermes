"""twin for hermes_cli/_scan_venv_blockers.py:_terminate_safe_preview (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/_scan_venv_blockers.py"
    PY_FN = "_terminate_safe_preview"
    C_FN = "_terminate_safe_preview"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
