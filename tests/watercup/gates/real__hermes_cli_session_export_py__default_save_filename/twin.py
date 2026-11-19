"""twin for hermes_cli/session_export.py:default_save_filename (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/session_export.py"
    PY_FN = "default_save_filename"
    C_FN = "default_save_filename"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
