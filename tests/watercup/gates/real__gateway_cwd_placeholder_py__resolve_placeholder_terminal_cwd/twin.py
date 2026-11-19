"""twin for gateway/cwd_placeholder.py:resolve_placeholder_terminal_cwd (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "gateway/cwd_placeholder.py"
    PY_FN = "resolve_placeholder_terminal_cwd"
    C_FN = "gateway_cwd_placeholder_resolve_placeholder_terminal_cwd"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
