"""twin for agent/agent_init.py:_resolve_compression_threshold (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_resolve_compression_threshold"
    C_FN = "agent_agent_init_u_resolve_compression_threshold"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
