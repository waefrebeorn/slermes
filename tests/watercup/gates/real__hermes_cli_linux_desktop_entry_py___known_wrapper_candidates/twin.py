"""twin for hermes_cli/linux_desktop_entry.py:_known_wrapper_candidates (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/linux_desktop_entry.py"
    PY_FN = "_known_wrapper_candidates"
    C_FN = "_known_wrapper_candidates"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
