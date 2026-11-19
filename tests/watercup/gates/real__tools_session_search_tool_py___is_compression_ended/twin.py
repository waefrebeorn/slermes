"""twin for tools/session_search_tool.py:_is_compression_ended (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/session_search_tool.py"
    PY_FN = "_is_compression_ended"
    C_FN = "tools_session_search_tool_u_is_compression_ended"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
