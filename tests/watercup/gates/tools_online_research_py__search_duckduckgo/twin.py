"""twin for tools/online_research.py:search_duckduckgo (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/online_research.py"
    PY_FN = "search_duckduckgo"
    C_FN = "tools_online_research_search_duckduckgo"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
