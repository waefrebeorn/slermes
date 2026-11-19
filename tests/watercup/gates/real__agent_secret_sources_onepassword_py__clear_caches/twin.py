"""twin for agent/secret_sources/onepassword.py:clear_caches (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/onepassword.py"
    PY_FN = "clear_caches"
    C_FN = "opw_clear_caches"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
