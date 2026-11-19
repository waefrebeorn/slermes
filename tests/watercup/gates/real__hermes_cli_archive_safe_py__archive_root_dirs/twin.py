"""twin for hermes_cli/archive_safe.py:archive_root_dirs (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/archive_safe.py"
    PY_FN = "archive_root_dirs"
    C_FN = "archive_root_dirs"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
