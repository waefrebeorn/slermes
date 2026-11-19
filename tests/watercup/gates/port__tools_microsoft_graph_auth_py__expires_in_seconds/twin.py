"""twin for tools/microsoft_graph_auth.py:expires_in_seconds (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/microsoft_graph_auth.py"
    PY_FN = "expires_in_seconds"
    C_FN = "cli_tools_microsoft_graph_auth_expires_in_seconds"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
