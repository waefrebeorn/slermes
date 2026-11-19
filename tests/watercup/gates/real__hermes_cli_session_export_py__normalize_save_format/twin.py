"""twin for hermes_cli/session_export.py:normalize_save_format (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export.py"
    PY_FN = "normalize_save_format"
    C_FN = "normalize_save_format"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
