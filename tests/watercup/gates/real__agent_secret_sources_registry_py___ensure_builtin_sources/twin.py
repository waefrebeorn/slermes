"""twin for agent/secret_sources/registry.py:_ensure_builtin_sources (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/secret_sources/registry.py"
    PY_FN = "_ensure_builtin_sources"
    C_FN = "ssr_ensure_builtin_sources"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
