"""twin for agent/agent_init.py:_normalize_custom_provider_name."""
from watercup_twin_runtime import TwinBase
class _Twin(TwinBase):
    PY_MODULE = "agent/agent_init.py"
    PY_FN = "_normalize_custom_provider_name"
    C_FN = "agent_agent_init_u_normalize_custom_provider_name"
    def norm(self, c, result):
        c.valstr("" if result is None else str(result))
_t = _Twin(); trace = _t.trace; run = _t.run
