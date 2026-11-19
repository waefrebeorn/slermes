"""twin for agent/context_compressor.py:_clear_compression_failure_cooldown (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/context_compressor.py"
    PY_FN = "_clear_compression_failure_cooldown"
    C_FN = "cli_agent_context_compressor__clear_compression_failure_cooldown"

    def norm(self, c, result):
        c.valstr("void")


_t = _Twin()
trace = _t.trace
run = _t.run
