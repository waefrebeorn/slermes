"""twin for hermes_cli/status.py:_format_iso_timestamp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/status.py"
    PY_FN = "_format_iso_timestamp"
    C_FN = "sta_format_iso_timestamp"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
