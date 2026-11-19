"""twin for tools/terminal_tool.py:_invalidate_cached_sudo_on_auth_failure (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/terminal_tool.py"
    PY_FN = "_invalidate_cached_sudo_on_auth_failure"
    C_FN = "tt_u_invalidate_cached_sudo_on_auth_failure"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
