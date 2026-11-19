"""twin for agent/context_compressor.py:get_active_compression_failure_cooldown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "get_active_compression_failure_cooldown"
    C_FN = "cli_agent_context_compressor__get_active_compression_failure_cooldown"

    def norm(self, c, result):
        c.valstr(str(int(result)) if isinstance(result, (int, bool)) and not isinstance(result, str) else ("0" if result is None else str(result)))


_t = _Twin()
trace = _t.trace
run = _t.run
