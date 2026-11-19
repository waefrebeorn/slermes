"""twin for tools/browser_tool.py:_agent_browser_candidate_present (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "tools/browser_tool.py"
    PY_FN = "_agent_browser_candidate_present"
    C_FN = "browser_agent_browser_candidate_present"

    def norm(self, c, result):
        c.valstr("true" if result else "false")


_t = _Twin()
trace = _t.trace
run = _t.run
