"""twin for tools/browser_use_cli.py:_resolve_real_profile_cdp (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_use_cli.py"
    PY_FN = "_resolve_real_profile_cdp"
    C_FN = "_resolve_real_profile_cdp"

    def norm(self, c, result):
        c.valstr(result if isinstance(result, str) else ("" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
