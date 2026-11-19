"""twin for agent/transports/codex.py:_default_prompt_cache_retention_for_request (thin: shared runtime, cross-OS)."""
from watercup_twin_runtime import TwinBase



class _Twin(TwinBase):
    PY_MODULE = "agent/transports/codex.py"
    PY_FN = "_default_prompt_cache_retention_for_request"
    C_FN = "_default_prompt_cache_retention_for_request"

    def norm(self, c, result):
        c.valstr(str(result))


_t = _Twin()
trace = _t.trace
run = _t.run
