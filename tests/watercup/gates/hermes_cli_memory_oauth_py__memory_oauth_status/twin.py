"""twin for hermes_cli/memory_oauth.py:memory_oauth_status (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "hermes_cli/memory_oauth.py"
    PY_FN = "memory_oauth_status"
    C_FN = "hermes_cli_memory_oauth_memory_oauth_status"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
