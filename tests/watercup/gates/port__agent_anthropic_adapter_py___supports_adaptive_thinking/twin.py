"""twin for agent/anthropic_adapter.py:_supports_adaptive_thinking (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/anthropic_adapter.py"
    PY_FN = "_supports_adaptive_thinking"
    C_FN = "cli_agent_anthropic_adapter__supports_adaptive_thinking"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
