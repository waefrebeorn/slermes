"""twin for agent/context_engine.py:prune_tool_results_only (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_engine.py"
    PY_FN = "prune_tool_results_only"
    C_FN = "agent_context_engine_prune_tool_results_only"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
